#include "master_discoverer.hh"

#include <array>
#include <cassert>
#include <ostream>

namespace {

    std::array<const char*,4> all_results {
        "add", "remove", "reject", "retain"
    };

}

std::ostream&
sbn::operator<<(std::ostream& out, probe_result rhs) {
    const size_t i = static_cast<size_t>(rhs);
    const char* s = i >= 0 && i <= all_results.size()
                    ? all_results[i] : "<unknown>";
    return out << s;
}

void
sbn::master_discoverer
::on_start() {
    this->probe_next_node();
}

void
sbn::master_discoverer
::on_kernel(sbn::kernel* k) {
    if (typeid(*k) == typeid(discovery_timer)) {
        // start probing only if it has not been started already
        if (this->state() == state_type::waiting) {
            this->probe_next_node();
        }
    } else if (typeid(*k) == typeid(probe)) {
        this->update_subordinates(dynamic_cast<probe*>(k));
    } else if (typeid(*k) == typeid(prober)) {
        this->update_principal(dynamic_cast<prober*>(k));
    } else if (typeid(*k) == typeid(socket_pipeline_kernel)) {
        this->on_event(dynamic_cast<socket_pipeline_kernel*>(k));
    } else if (typeid(*k) == typeid(hierarchy_kernel)) {
        this->update_weights(dynamic_cast<hierarchy_kernel*>(k));
    }
}

void
sbn::master_discoverer
::probe_next_node() {
    this->setstate(state_type::probing);
    if (this->_iterator == this->_end) {
        this->_iterator = iterator(this->interface_address(), this->_fanout);
        this->log("_: all addresses have been probed", this->interface_address());
        this->send_timer();
    } else {
        addr_type addr = *this->_iterator;
        sys::socket_address new_principal(addr, this->port());
        this->log("_: probe _", this->interface_address(), addr);
        prober* p =
            new prober(
                this->interface_address(),
                this->_hierarchy.principal().socket_address(),
                new_principal
            );
        sbn::upstream(this, p);
        ++this->_iterator;
    }
}

void
sbn::master_discoverer
::send_timer() {
    this->setstate(state_type::waiting);
    using namespace std::chrono;
    discovery_timer* k = new discovery_timer;
    k->after(this->_interval);
    sbn::send(k, this);
}

void
sbn::master_discoverer
::update_subordinates(probe* p) {
    const sys::socket_address src = p->from();
    probe_result result = this->process_probe(p);
    this->log("_: _ subordinate _", this->interface_address(), result, src);
    bool changed = true;
    if (result == probe_result::add_subordinate) {
        this->_hierarchy.add_subordinate(src);
    } else if (result == probe_result::remove_subordinate) {
        this->_hierarchy.remove_subordinate(src);
    } else {
        changed = false;
    }
    if (changed) {
        this->broadcast_hierarchy();
    }
    p->setf(kernel_flag::do_not_delete);
    sbn::commit<sbn::Remote>(p);
}

sbn::probe_result
sbn::master_discoverer
::process_probe(probe* p) {
    probe_result result = probe_result::retain;
    if (p->from() == this->_hierarchy.principal()) {
        // principal tries to become subordinate
        // which is prohibited
        p->return_code(exit_code::error);
        result = probe_result::reject_subordinate;
    } else {
        if (p->old_principal() != p->new_principal()) {
            if (p->new_principal() == this->_hierarchy.socket_address()) {
                result = probe_result::add_subordinate;
            } else if (p->old_principal() == this->_hierarchy.socket_address()) {
                result = probe_result::remove_subordinate;
            }
        }
        p->return_code(exit_code::success);
    }
    return result;
}

void
sbn::master_discoverer
::update_principal(prober* p) {
    if (p->return_code() != exit_code::success) {
        this->log(
            "_: prober returned from _: _",
            this->interface_address(),
            p->new_principal(),
            p->return_code()
        );
        this->probe_next_node();
    } else {
        const sys::socket_address& oldp = p->old_principal();
        const sys::socket_address& newp = p->new_principal();
        if (oldp) {
            ::sbn::factory.nic().stop_client(oldp);
        }
        this->log("_: set principal to _", this->interface_address(), newp);
        this->_hierarchy.set_principal(newp);
        this->broadcast_hierarchy();
        // try to find better principal after a period of time
        this->send_timer();
    }
}

void
sbn::master_discoverer
::on_event(socket_pipeline_kernel* ev) {
    switch (ev->event()) {
    case socket_pipeline_event::add_client:
        this->on_client_add(ev->socket_address());
        break;
    case socket_pipeline_event::remove_client:
        this->on_client_remove(ev->socket_address());
        break;
    case socket_pipeline_event::add_server:
    case socket_pipeline_event::remove_server:
    default:
        // ignore server events
        break;
    }
}

void
sbn::master_discoverer
::on_client_add(const sys::socket_address& endp) {}

void
sbn::master_discoverer
::on_client_remove(const sys::socket_address& endp) {
    if (endp == this->_hierarchy.principal()) {
        this->log("_: unset principal _", this->interface_address(), endp);
        this->_hierarchy.unset_principal();
        this->probe_next_node();
    } else {
        this->log("_: remove subordinate _", this->interface_address(), endp);
        this->_hierarchy.remove_subordinate(endp);
    }
}

void
sbn::master_discoverer
::broadcast_hierarchy(
    sys::socket_address
    ignored_endpoint
) {
    const weight_type total = this->_hierarchy.total_weight();
    for (const hierarchy_node& sub : this->_hierarchy) {
        if (sub.socket_address() != ignored_endpoint) {
            assert(total >= sub.weight());
            this->send_weight(sub.socket_address(), total - sub.weight());
        }
    }
    if (this->_hierarchy.has_principal()) {
        const hierarchy_node& princ = this->_hierarchy.principal();
        if (princ.socket_address() != ignored_endpoint) {
            assert(total >= princ.weight());
            this->send_weight(princ.socket_address(), total - princ.weight());
        }
    }
}

void
sbn::master_discoverer
::send_weight(const sys::socket_address& dest, weight_type w) {
    hierarchy_kernel* h = new hierarchy_kernel(this->interface_address(), w);
    h->parent(this);
    h->set_principal_id(1);
    h->to(dest);
    sbn::send<sbn::Remote>(h);
}

void
sbn::master_discoverer
::update_weights(hierarchy_kernel* k) {
    if (k->moves_downstream() && k->return_code() != exit_code::success) {
        this->log(
            "_: failed to send hierarchy to _",
            this->interface_address(),
            k->from()
        );
    } else {
        const sys::socket_address& src = k->from();
        bool changed = false;
        if (this->_hierarchy.has_principal(src)) {
            changed = this->_hierarchy.set_principal_weight(k->weight());
        } else if (this->_hierarchy.has_subordinate(src)) {
            changed = this->_hierarchy.set_subordinate_weight(src, k->weight());
        }
        this->log(
            "_: set _ weight to _",
            this->interface_address(),
            k->from(),
            k->weight()
        );
        if (changed) {
            ::sbn::factory.nic().set_client_weight(src, k->weight());
            this->broadcast_hierarchy(src);
        }
    }
}
