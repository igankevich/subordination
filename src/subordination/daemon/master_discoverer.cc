#include <array>
#include <cassert>
#include <ostream>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/master_discoverer.hh>

namespace {

    constexpr const std::array<const char*,4> all_results {
        "add", "remove", "reject", "retain"
    };

}

std::ostream&
sbnd::operator<<(std::ostream& out, probe_result rhs) {
    const size_t i = static_cast<size_t>(rhs);
    const char* s = i <= all_results.size() ? all_results[i] : "<unknown>";
    return out << s;
}

void sbnd::master_discoverer::on_start() {
    this->probe_next_node();
}

void sbnd::master_discoverer::on_kernel(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(discovery_timer)) {
        // start probing only if it has not been started already
        if (this->state() == state_type::waiting) {
            this->probe_next_node();
        }
    } else if (typeid(*k) == typeid(probe)) {
        this->update_subordinates(sbn::pointer_dynamic_cast<probe>(std::move(k)));
    } else if (typeid(*k) == typeid(prober)) {
        this->update_principal(sbn::pointer_dynamic_cast<prober>(std::move(k)));
    } else if (typeid(*k) == typeid(socket_pipeline_kernel)) {
        this->on_event(sbn::pointer_dynamic_cast<socket_pipeline_kernel>(std::move(k)));
    } else if (typeid(*k) == typeid(Hierarchy_kernel)) {
        this->update_weights(sbn::pointer_dynamic_cast<Hierarchy_kernel>(std::move(k)));
    }
}

void sbnd::master_discoverer::probe_next_node() {
    this->setstate(state_type::probing);
    if (this->_iterator == this->_end) {
        this->_iterator = iterator(this->interface_address(), this->_fanout);
        this->log("_: all addresses have been probed", this->interface_address());
        this->send_timer();
    } else {
        addr_type addr = *this->_iterator;
        sys::socket_address new_principal(addr, this->port());
        const auto& old_principal = this->_hierarchy.principal().socket_address();
        if (new_principal != old_principal) {
            this->log("_: probe _", this->interface_address(), addr);
            auto p = sbn::make_pointer<prober>(this->interface_address(), old_principal,
                                               new_principal);
            p->parent(this);
            factory.local().send(std::move(p));
        }
        ++this->_iterator;
    }
}

void sbnd::master_discoverer::send_timer() {
    this->setstate(state_type::waiting);
    using namespace std::chrono;
    auto k = sbn::make_pointer<discovery_timer>();
    k->after(this->_interval);
    k->principal(this);
    k->phase(phases::point_to_point);
    factory.local().send(std::move(k));
}

void sbnd::master_discoverer::update_subordinates(pointer<probe> p) {
    const sys::socket_address src = p->source();
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
    p->return_to_parent(sbn::exit_code::success);
    factory.remote().send(std::move(p));
}

sbnd::probe_result sbnd::master_discoverer::process_probe(pointer<probe>& p) {
    probe_result result = probe_result::retain;
    if (p->source() == this->_hierarchy.principal()) {
        // principal tries to become subordinate
        // which is prohibited
        p->return_code(sbn::exit_code::error);
        result = probe_result::reject_subordinate;
    } else {
        if (p->old_principal() != p->new_principal()) {
            if (p->new_principal() == this->_hierarchy.socket_address()) {
                result = probe_result::add_subordinate;
            } else if (p->old_principal() == this->_hierarchy.socket_address()) {
                result = probe_result::remove_subordinate;
            }
        }
        p->return_code(sbn::exit_code::success);
    }
    return result;
}

void sbnd::master_discoverer::update_principal(pointer<prober> p) {
    if (p->return_code() != sbn::exit_code::success) {
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
        if (oldp) { factory.remote().stop_client(oldp); }
        this->log("_: set principal to _", this->interface_address(), newp);
        this->_hierarchy.set_principal(newp);
        this->broadcast_hierarchy();
        // try to find better principal after a period of time
        this->send_timer();
    }
}

void
sbnd::master_discoverer::on_event(pointer<socket_pipeline_kernel> ev) {
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
sbnd::master_discoverer
::on_client_add(const sys::socket_address& endp) {}

void
sbnd::master_discoverer
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

void sbnd::master_discoverer::broadcast_hierarchy(sys::socket_address ignored_endpoint) {
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

void sbnd::master_discoverer::send_weight(const sys::socket_address& dest, weight_type w) {
    auto h = sbn::make_pointer<Hierarchy_kernel>(this->interface_address(), w);
    h->point_to_point(1);
    h->parent(this);
    h->destination(dest);
    factory.remote().send(std::move(h));
}

void sbnd::master_discoverer::update_weights(pointer<Hierarchy_kernel> k) {
    if (k->phase() == kernel::phases::downstream &&
        k->return_code() != sbn::exit_code::success) {
        log("_: failed to send hierarchy to _", interface_address(), k->source());
    } else {
        const sys::socket_address& src = k->source();
        bool changed = false;
        if (this->_hierarchy.has_principal(src)) {
            changed = this->_hierarchy.set_principal_weight(k->weight());
        } else if (this->_hierarchy.has_subordinate(src)) {
            changed = this->_hierarchy.set_subordinate_weight(src, k->weight());
        }
        log("_: set _ weight to _", interface_address(), k->source(), k->weight());
        if (changed) {
            factory.remote().set_client_weight(src, k->weight());
            this->broadcast_hierarchy(src);
        }
    }
}
