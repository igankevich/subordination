#include <array>
#include <cassert>
#include <ostream>
#include <sstream>

#include <subordination/daemon/discoverer.hh>
#include <subordination/daemon/factory.hh>

namespace {

    constexpr const std::array<const char*,4> all_results {
        "add", "remove", "reject", "retain"
    };

    inline uint64_t current_time_in_microseconds() {
        using namespace std::chrono;
        const auto now = system_clock::now().time_since_epoch();
        const auto t = duration_cast<microseconds>(now);
        return t.count();
    }

    /// Timer which is used to periodically scan nodes
    /// to find the best principal node.
    class discovery_timer: public sbn::kernel {};

}

std::ostream&
sbnd::operator<<(std::ostream& out, probe_result rhs) {
    const size_t i = static_cast<size_t>(rhs);
    const char* s = i <= all_results.size() ? all_results[i] : "<unknown>";
    return out << s;
}

void sbnd::discoverer::on_start() {
    discover();
}

void sbnd::discoverer::on_kernel(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(discovery_timer)) {
        on_timer();
    } else if (typeid(*k) == typeid(probe)) {
        switch (k->phase()) {
            case sbn::kernel::phases::downstream:
                update_superior(sbn::pointer_dynamic_cast<probe>(std::move(k)));
                break;
            case sbn::kernel::phases::point_to_point:
                update_subordinates(sbn::pointer_dynamic_cast<probe>(std::move(k)));
                break;
            default: break;
        }
    } else if (typeid(*k) == typeid(socket_pipeline_kernel)) {
        on_event(sbn::pointer_dynamic_cast<socket_pipeline_kernel>(std::move(k)));
    } else if (typeid(*k) == typeid(Hierarchy_kernel)) {
        update_weights(sbn::pointer_dynamic_cast<Hierarchy_kernel>(std::move(k)));
    }
}

void sbnd::discoverer::discover() {
    if (this->_iterator == this->_end) {
        reset_iterator();
        log("_: all addresses have been probed", interface_address());
        discover_later();
    } else {
        auto addr = *this->_iterator;
        sys::socket_address new_superior(sys::ipv4_socket_address{addr, port()});
        const auto& old_superior = this->_hierarchy.superior_socket_address();
        if (profile()) {
            profile("`((time . _) (node . \"_\") (probe . \"_\") (attempts . _))",
                    current_time_in_microseconds(), interface_address(), addr,
                    this->_attempts);
        } else {
            log("_: probe _ attempts _", interface_address(), addr, this->_attempts);
        }
        auto p = sbn::make_pointer<probe>(interface_address(), old_superior, new_superior);
        p->parent(this);
        p->destination(new_superior);
        p->principal_id(1); // TODO
        p->phase(sbn::kernel::phases::point_to_point);
        factory.remote().send(std::move(p));
        if (++this->_attempts >= max_attempts()) {
            this->_attempts = 0;
            ++this->_iterator;
        }
    }
}

void sbnd::discoverer::on_timer() {
    if (state() != states::waiting) { return; }
    if (this->_hierarchy.has_superior()) { reset_iterator(); }
    sys::socket_address new_superior(sys::ipv4_socket_address{*this->_iterator, port()});
    const auto& old_superior = this->_hierarchy.superior_socket_address();
    if (old_superior != new_superior) { discover(); }
    else { discover_later(); }
}

void sbnd::discoverer::reset_iterator() {
    this->_iterator = iterator(interface_address(), this->_fanout);
}

void sbnd::discoverer::discover_later() {
    state(states::waiting);
    using namespace std::chrono;
    auto k = sbn::make_pointer<discovery_timer>();
    k->after(this->_interval);
    k->principal(this);
    k->phase(phases::point_to_point);
    factory.local().send(std::move(k));
}

void sbnd::discoverer::update_subordinates(pointer<probe> p) {
    const sys::socket_address src = p->source();
    probe_result result = this->process_probe(p);
    if (profile()) {
        profile("`((time . _) (node . \"_\") (operation . _) (subordinate . \"_\"))",
                current_time_in_microseconds(), interface_address(), result, src);
    } else {
        #if defined(SBN_TEST)
        sys::log_message("test", "_: _ subordinate _", this->interface_address(), result, src);
        #endif
    }
    resource_array total_resources;
    if (result == probe_result::add_subordinate) {
        total_resources = this->_hierarchy.total_resources();
        add_subordinate(src);
    } else if (result == probe_result::remove_subordinate) {
        remove_subordinate(src);
        total_resources = this->_hierarchy.total_resources();
    }
    p->return_to_parent(sbn::exit_code::success);
    hierarchy_node sup;
    sup.resources(total_resources);
    p->superior(sup);
    factory.remote().send(std::move(p));
}

void sbnd::discoverer::add_subordinate(const sys::socket_address& address) {
    if (this->_hierarchy.add_subordinate(address)) {
        broadcast_hierarchy(address);
    }
}

void sbnd::discoverer::add_superior(const sys::socket_address& address,
                                    const resource_array& resources) {
    if (this->_hierarchy.add_superior(address, hierarchy_node(resources))) {
        factory.remote().update_client(address, resources);
        broadcast_hierarchy(address);
    }
}

void sbnd::discoverer::remove_subordinate(const sys::socket_address& address) {
    if (this->_hierarchy.remove_subordinate(address)) {
        broadcast_hierarchy();
    }
}

void sbnd::discoverer::remove_superior() {
    if (this->_hierarchy.remove_superior()) {
        broadcast_hierarchy();
    }
}

sbnd::probe_result sbnd::discoverer::process_probe(pointer<probe>& p) {
    probe_result result = probe_result::retain;
    if (p->source() == this->_hierarchy.superior_socket_address()) {
        // superior tries to become subordinate
        // which is prohibited
        p->return_code(sbn::exit_code::error);
        result = probe_result::reject_subordinate;
    } else {
        if (p->old_superior() != p->new_superior()) {
            if (p->new_superior() == this->_hierarchy.socket_address()) {
                result = probe_result::add_subordinate;
            } else if (p->old_superior() == this->_hierarchy.socket_address()) {
                result = probe_result::remove_subordinate;
            }
        }
        p->return_code(sbn::exit_code::success);
    }
    return result;
}

void sbnd::discoverer::update_superior(pointer<probe> p) {
    if (p->return_code() != sbn::exit_code::success) {
        this->log("_: probe returned from _: _", this->interface_address(),
                  p->new_superior(), p->return_code());
        discover();
    } else {
        const auto& old_superior = p->old_superior(), new_superior = p->new_superior();
        if (old_superior != new_superior) {
            if (old_superior) { factory.remote().stop_client(old_superior); }
            if (profile()) {
                profile("`((time . _) (node . \"_\") (superior . \"_\") (attempts . _))",
                        current_time_in_microseconds(), interface_address(),
                        new_superior, this->_attempts);
            } else {
                #if defined(SBN_TEST)
                sys::log_message("test", "_: set principal to _ attempts _ weight _",
                                 interface_address(), new_superior, this->_attempts,
                                 p->superior().num_threads());
                #endif
            }
            add_superior(new_superior, p->superior().resources());
        }
        if (p->old_superior() && p->old_superior() != p->new_superior()) {
            auto new_p = sbn::make_pointer<probe>(p->interface_address(), p->old_superior(),
                                                  p->new_superior());
            new_p->parent(this);
            new_p->destination(p->old_superior());
            new_p->principal_id(1); // TODO
            new_p->phase(sbn::kernel::phases::point_to_point);
            factory.remote().send(std::move(new_p));
        }
        // try to find better superior after a period of time
        discover_later();
    }
}

void
sbnd::discoverer::on_event(pointer<socket_pipeline_kernel> ev) {
    using e = socket_pipeline_event;
    switch (ev->event()) {
        case e::add_client: on_client_add(ev->socket_address()); break;
        case e::remove_client: on_client_remove(ev->socket_address()); break;
        case e::add_server: break;
        case e::remove_server: break;
        default: break;
    }
}

void
sbnd::discoverer::on_client_add(const sys::socket_address& address) {}

void
sbnd::discoverer::on_client_remove(const sys::socket_address& address) {
    if (address == this->_hierarchy.superior_socket_address()) {
        #if defined(SBN_TEST)
        sys::log_message("test", "_: unset principal _", interface_address(), address);
        #endif
        remove_superior();
        discover();
    } else {
        #if defined(SBN_TEST)
        sys::log_message("test", "_: remove subordinate _", interface_address(), address);
        #endif
        remove_subordinate(address);
    }
}

void sbnd::discoverer::broadcast_hierarchy(sys::socket_address ignored_endpoint) {
    auto total = this->_hierarchy.total_resources();
    for (const auto& pair : this->_hierarchy.subordinates()) {
        const auto& sub_socket_address = pair.first;
        const auto& sub = pair.second;
        if (sub_socket_address != ignored_endpoint) {
            //assert(total >= sub.weight());
            total -= sub.resources();
            this->send_weight(sub_socket_address, total);
        }
    }
    if (this->_hierarchy.has_superior()) {
        const auto& sup_socket_address = this->_hierarchy.superior_socket_address();
        const hierarchy_node& sup = this->_hierarchy.superior();
        if (sup_socket_address != ignored_endpoint) {
            //assert(total >= sup.weight());
            total -= sup.resources();
            this->send_weight(sup_socket_address, total);
        }
    }
    write_cache();
}

std::string sbnd::discoverer::cache_filename() const {
    std::stringstream tmp;
    tmp << this->_hierarchy.interface_address().address();
    tmp << '-';
    tmp << this->_hierarchy.interface_address().prefix();
    tmp << '-';
    tmp << this->_hierarchy.port();
    return tmp.str();
}

void sbnd::discoverer::write_cache() const {
    using f = sys::open_flag;
    sys::fildes out;
    try {
        sys::path path(cache_directory(), cache_filename());
        out.open(path, f::truncate | f::close_on_exec | f::create | f::write_only, 0600);
        sbn::kernel_buffer buf;
        buf << this->_hierarchy;
        buf.flip();
        buf.flush(out);
        out.close();
        log("write hierarchy to _: _", path, this->_hierarchy);
    } catch (const sys::bad_call& err) {
        log("failed to write cache: _", err.what());
    }
}

void sbnd::discoverer::read_cache() {
    using f = sys::open_flag;
    sys::fildes in;
    try {
        sys::path path(cache_directory(), cache_filename());
        in.open(path, f::close_on_exec | f::read_only);
        sbn::kernel_buffer buf;
        buf.fill(in);
        buf.flip();
        buf >> this->_hierarchy;
        in.close();
        log("read hierarchy from _: _", path, this->_hierarchy);
        if (this->_hierarchy.has_superior()) {
            const auto& sup = this->_hierarchy.superior();
            const auto& sup_socket_address = this->_hierarchy.superior_socket_address();
            auto g = factory.remote().guard();
            factory.remote().add_client(sup_socket_address, sup.resources());
        }
        //for (const auto& s : this->_hierarchy.subordinates()) {
        //    auto g = factory.remote().guard();
        //    factory.remote().add_client(s.socket_address(), s.weight());
        //}
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::no_such_file_or_directory) {
            log("failed to read cache: _", err.what());
        }
    }
}

void sbnd::discoverer::send_weight(const sys::socket_address& dest,
                                   const resource_array& resources) {
    auto h = sbn::make_pointer<Hierarchy_kernel>(this->interface_address(), resources);
    h->point_to_point(1);
    h->parent(this);
    h->destination(dest);
    factory.remote().send(std::move(h));
}

void sbnd::discoverer::update_weights(pointer<Hierarchy_kernel> k) {
    if (k->phase() == kernel::phases::downstream &&
        k->return_code() != sbn::exit_code::success) {
        log("_: failed to send hierarchy to _: _", interface_address(), k->source(),
            k->return_code());
    } else {
        const sys::socket_address& src = k->source();
        bool changed = false;
        if (this->_hierarchy.has_superior(src)) {
            changed = this->_hierarchy.set_superior(hierarchy_node(k->resources()));
        } else if (this->_hierarchy.has_subordinate(src)) {
            changed = this->_hierarchy.set_subordinate(src, hierarchy_node(k->resources()));
        }
        if (changed) {
            #if defined(SBN_TEST)
            sys::log_message("test", "_: set _ weight to _",
                             interface_address(), k->source(), k->resources()[1]);
            #endif
            factory.remote().update_client(src, k->resources());
            broadcast_hierarchy(src);
        }
    }
}

sbnd::discoverer::discoverer(const ifaddr_type& interface_address,
                             const sys::port_type port,
                             const Properties::Discoverer& props):
_fanout(props.fanout), _hierarchy(interface_address, port) {
    this->_interval = props.scan_interval;
    this->_profile = props.profile;
    this->_max_attempts = props.max_attempts;
    this->_cache_directory = props.cache_directory;
    reset_iterator();
}

void sbnd::discoverer::resources(const sbn::resource_array& rhs) {
    this->_hierarchy.resources(rhs);
    auto g = factory.remote().guard();
    factory.remote().scheduler().local_resources(rhs);
}
