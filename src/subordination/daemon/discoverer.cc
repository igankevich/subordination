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
                probe_returned(sbn::pointer_dynamic_cast<probe>(std::move(k)));
                break;
            case sbn::kernel::phases::point_to_point:
                probe_received(sbn::pointer_dynamic_cast<probe>(std::move(k)));
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
    log("hierarchy _", this->_hierarchy);
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
        p->nodes({this->_hierarchy.this_node()});
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

void sbnd::discoverer::probe_received(pointer<probe> p) {
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
    const auto& nodes = p->nodes();
    if (!nodes.empty()) {
        const auto now = hierarchy_node::clock::now();
        bool changed = this->_hierarchy.add_nodes(nodes, now);
        if (result == probe_result::add_subordinate) {
            changed |= this->_hierarchy.add_subordinate(nodes.front(), now);
        } else if (result == probe_result::remove_subordinate) {
            changed |= this->_hierarchy.remove_node(nodes.front().socket_address(), now);
        }
        if (changed) {
            broadcast_hierarchy(src);
        }
    }
    p->return_to_parent(sbn::exit_code::success);
    p->nodes(this->_hierarchy.nodes(this->_max_radius));
    factory.remote().send(std::move(p));
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

void sbnd::discoverer::probe_returned(pointer<probe> p) {
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
                                 p->nodes().front().total_threads());
                #endif
            }
            bool changed = false;
            const auto& nodes = p->nodes();
            if (!nodes.empty()) {
                const auto now = hierarchy_node::clock::now();
                changed |= this->_hierarchy.add_nodes(nodes, now);
                changed |= this->_hierarchy.add_superior(nodes.front(), now);
                if (changed) {
                    update_socket_pipeline_clients(nodes);
                    broadcast_hierarchy(p->nodes().front().socket_address());
                }
            }
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

void sbnd::discoverer::update_socket_pipeline_clients(const hierarchy_node_array& nodes) {
    auto g = factory.remote().guard();
    factory.remote().update_clients(this->_hierarchy);
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
    const auto now = hierarchy_node::clock::now();
    if (this->_hierarchy.remove_node(address, now)) {
        broadcast_hierarchy(address);
    }
    if (address == this->_hierarchy.superior_socket_address()) {
        #if defined(SBN_TEST)
        sys::log_message("test", "_: unset principal _", interface_address(), address);
        #endif
        discover();
    } else {
        #if defined(SBN_TEST)
        sys::log_message("test", "_: remove subordinate _", interface_address(), address);
        #endif
    }
}

void sbnd::discoverer::broadcast_hierarchy(const sys::socket_address& ignored_endpoint) {
    auto nodes = this->_hierarchy.nodes(this->_max_radius);
    for (const auto& pair : this->_hierarchy.nodes()) {
        const auto& sub_socket_address = pair.first;
        if (sub_socket_address != ignored_endpoint) {
            send_weight(sub_socket_address, nodes);
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
        if (auto* sup = this->_hierarchy.superior()) {
            auto g = factory.remote().guard();
            factory.remote().add_client(sup->socket_address(), this->_hierarchy);
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
                                   const hierarchy_node_array& nodes) {
    auto h = sbn::make_pointer<Hierarchy_kernel>(interface_address(), nodes);
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
        const auto now = hierarchy_node::clock::now();
        if (this->_hierarchy.add_nodes(k->nodes(), now)) {
            const auto& src = k->source();
            #if defined(SBN_TEST)
            sys::log_message("test", "_: set _ weight to _",
                             interface_address(), k->source(),
                             k->nodes().front().resources()[1]);
            #endif
            update_socket_pipeline_clients(k->nodes());
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
    this->_max_radius = props.max_radius;
    reset_iterator();
}

void sbnd::discoverer::resources(const sbn::resource_array& rhs,
                                 hierarchy_node::time_point now) {
    if (this->_hierarchy.resources(rhs, now)) {
        {
            auto g = factory.remote().guard();
            factory.remote().scheduler().local_resources(rhs);
        }
        broadcast_hierarchy({});
    }
}
