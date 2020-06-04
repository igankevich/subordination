#include <subordination/core/application.hh>
#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_instance_registry.hh>

namespace  {

    template <class Queue>
    inline typename Queue::const_iterator
    find_kernel(sbn::kernel* k, const Queue& queue) {
        return std::find_if(queue.begin(), queue.end(),
                            [k] (sbn::kernel* rhs) { return rhs->id() == k->id(); });
    }

    inline void send_or_delete(sbn::pipeline* ppl, sbn::kernel* k, bool& remove) {
        if (ppl) { ppl->send(k); }
        else { remove = true; }
    };

    inline void forward_or_delete(sbn::pipeline* ppl, sbn::kernel* k, bool& remove) {
        if (ppl) { ppl->forward(dynamic_cast<sbn::foreign_kernel*>(k)); }
        else { remove = true; }
    };

    inline void forward_or_delete(sbn::pipeline* ppl, sbn::foreign_kernel* k, bool& remove) {
        if (ppl) { ppl->forward(k); }
        else { remove = true; }
    };

}

void sbn::connection::handle(const sys::epoll_event& event) {
    constexpr const size_t n = 20;
    char tmp[n];
    ssize_t c;
    while ((c = ::read(event.fd(), tmp, n)) != -1) ;
}

void sbn::connection::remove(sys::event_poller& poller) {}
void sbn::connection::flush() {}

void sbn::connection::send(kernel* k) {
    // return local downstream kernels immediately
    // TODO we need to move some kernel flags to
    // kernel header in order to use them in routing
    if (k->moves_downstream() && !k->to()) {
        if (k->isset(kernel_flag::parent_is_id) || k->carries_parent()) {
            if (k->carries_parent()) {
                delete k->parent();
            }
            this->plug_parent(k);
        }
        #if defined(SBN_DEBUG)
        this->log("send local kernel _", *k);
        #endif
        this->parent()->native_pipeline()->send(k);
        return;
    }
    bool delete_kernel = this->save_kernel(k);
    #if defined(SBN_DEBUG)
    this->log("send _ to _", *k, this->_socket_address);
    #endif
    this->write_kernel(k);
    /// The kernel is deleted if it goes downstream
    /// and does not carry its parent.
    if (delete_kernel) {
        if (k->moves_downstream() && k->carries_parent()) {
            delete k->parent();
        }
        delete k;
    }
}

void sbn::connection::forward(foreign_kernel* k) {
    bool delete_kernel = this->save_kernel(k);
    (*this->_stream).begin_packet();
    (*this->_stream) << k->header();
    (*this->_stream) << *k;
    (*this->_stream).end_packet();
    if (delete_kernel) {
        delete k;
    }
}

void sbn::connection::write_kernel(kernel* k) noexcept {
    using opacket_guard = sys::opacket_guard<kstream>;
    try {
        auto& s = (*this->_stream);
        opacket_guard g(s);
        log("xxx _", typeid(*this).name());
        assert((*this->_stream).rdbuf());
        s.begin_packet();
        if (this->_flags & kernel_proto_flag::prepend_source_and_destination) {
            k->header().prepend_source_and_destination();
        }
        s << k->header();
        s << *k;
        s.end_packet();
    } catch (const kernel_error& err) {
        log_write_error(err);
    } catch (const error& err) {
        log_write_error(err);
    } catch (const std::exception& err) {
        log_write_error(err.what());
    } catch (...) {
        log_write_error("<unknown>");
    }
}

void sbn::connection::receive_kernels(const application* from_application,
                                         std::function<void(kernel*)> func) {
    while ((*this->_stream).read_packet()) {
        try {
            if (auto* k = this->read_kernel(from_application)) {
                bool ok = this->receive_kernel(k);
                func(k);
                if (!ok) {
                    #if defined(SBN_DEBUG)
                    this->log("no principal found for _", *k);
                    #endif
                    k->principal(k->parent());
                    this->send(k);
                } else {
                    this->_parent->native_pipeline()->send(k);
                }
            }
        } catch (const kernel_error& err) {
            log_read_error(err);
        } catch (const error& err) {
            log_read_error(err);
        } catch (const std::exception& err) {
            log_read_error(err.what());
        } catch (...) {
            log_read_error("<unknown>");
        }
    }
}

sbn::kernel*
sbn::connection::read_kernel(const application* from_application) {
    using ipacket_guard = typename kstream::ipacket_guard;
    // eats remaining bytes on exception
    ipacket_guard g((*this->_stream).rdbuf());
    foreign_kernel* hdr = new foreign_kernel;
    kernel* k = nullptr;
    (*this->_stream) >> hdr->header();
    if (from_application) {
        hdr->setapp(from_application->id());
        hdr->aptr(from_application);
    }
    if (this->_socket_address) {
        hdr->from(this->_socket_address);
        hdr->prepend_source_and_destination();
    }
    #if defined(SBN_DEBUG)
    this->log("recv _", hdr->header());
    #endif
    bool remove = false;
    if (hdr->app() != this_application::get_id()) {
        (*this->_stream) >> *hdr;
        forward_or_delete(this->parent()->foreign_pipeline(), hdr, remove);
    } else {
        (*this->_stream) >> k;
        k->setapp(hdr->app());
        if (hdr->has_source_and_destination()) {
            k->from(hdr->from());
            k->to(hdr->to());
        } else {
            k->from(this->_socket_address);
        }
        if (k->carries_parent()) {
            k->parent()->setapp(hdr->app());
        }
        remove = true;
    }
    if (remove) { delete hdr; }
    return k;
}

bool sbn::connection::receive_kernel(kernel* k) {
    bool ok = true;
    if (k->moves_downstream()) {
        this->plug_parent(k);
    } else if (k->principal_id()) {
        instances_guard g(instances);
        auto result = instances.find(k->principal_id());
        if (result == instances.end()) {
            k->return_code(exit_code::no_principal_found);
            ok = false;
        }
        k->principal(result->second);
    }
    #if defined(SBN_DEBUG)
    this->log("recv _", *k);
    #endif
    return ok;
}

void sbn::connection::plug_parent(kernel* k) {
    if (!k->has_id()) {
        throw std::invalid_argument("downstream kernel without an id");
    }
    auto pos = find_kernel(k, this->_upstream);
    if (pos == this->_upstream.end()) {
        if (k->carries_parent()) {
            k->principal(k->parent());
            this->log("recover parent for _", *k);
            auto result2 = find_kernel(k, this->_downstream);
            if (result2 != this->_downstream.end()) {
                kernel* old = *result2;
                this->log("delete _", *old);
                delete old->parent();
                delete old;
                this->_downstream.erase(result2);
            }
        } else {
            this->log("parent not found for _", *k);
            delete k;
            throw std::invalid_argument("parent not found");
        }
    } else {
        kernel* orig = *pos;
        k->parent(orig->parent());
        k->principal(k->parent());
        delete orig;
        this->_upstream.erase(pos);
        #if defined(SBN_DEBUG)
        this->log("plug parent for _", *k);
        #endif
    }
}

bool sbn::connection::save_kernel(kernel* k) {
    bool delete_kernel = false;
    if ((this->_flags & kernel_proto_flag::save_upstream_kernels) &&
        (k->moves_upstream() || k->moves_somewhere())) {
        if (k->is_native()) {
            this->ensure_has_id(k->parent());
            this->ensure_has_id(k);
        }
        #if defined(SBN_DEBUG)
        this->log("save parent for _", *k);
        #endif
        this->_upstream.push_back(k);
    } else if ((this->_flags & kernel_proto_flag::save_downstream_kernels) &&
               k->moves_downstream() && k->carries_parent()) {
        #if defined(SBN_DEBUG)
        this->log("save parent for _", *k);
        #endif
        this->_downstream.push_back(k);
    } else if (!k->moves_everywhere()) {
        delete_kernel = true;
    }
    return delete_kernel;
}

void sbn::connection::recover_kernels(bool down) {
    #if defined(SBN_DEBUG)
    this->log("recover kernels upstream _ downstream _",
              this->_upstream.size(), this->_downstream.size());
    #endif
    kernel_queue* queues[2] { &this->_upstream, &this->_downstream };
    const int n = down ? 2 : 1;
    for (int i=0; i<n; ++i) {
        auto& queue = *queues[i];
        while (!queue.empty()) {
            auto* k = queue.front();
            queue.pop_front();
            try {
                this->recover_kernel(k);
            } catch (const std::exception& err) {
                this->log("failed to recover kernel _", *k);
                delete k;
            }
        }
    }
}

void sbn::connection::recover_kernel(kernel* k) {
    bool remove = false;
    #if defined(SBN_DEBUG)
    this->log("try to recover _", k->id());
    #endif
    const bool native = k->is_native();
    if (k->moves_upstream() && !k->to()) {
        #if defined(SBN_DEBUG)
        this->log("recover _", *k);
        #endif
        if (native) {
            send_or_delete(this->parent()->remote_pipeline(), k, remove);
        } else {
            forward_or_delete(this->parent()->remote_pipeline(), k, remove);
        }
    } else if (k->moves_somewhere() || (k->moves_upstream() && k->to())) {
        #if defined(SBN_DEBUG)
        this->log("destination is unreachable for _", *k);
        #endif
        k->from(k->to());
        k->return_code(exit_code::endpoint_not_connected);
        k->principal(k->parent());
        if (native) {
            send_or_delete(this->parent()->native_pipeline(), k, remove);
        } else {
            forward_or_delete(this->parent()->foreign_pipeline(), k, remove);
        }
    } else if (k->moves_downstream() && k->carries_parent()) {
        #if defined(SBN_DEBUG)
        this->log("restore parent _", *k);
        #endif
        if (native) {
            send_or_delete(this->parent()->native_pipeline(), k, remove);
        } else {
            forward_or_delete(this->parent()->foreign_pipeline(), k, remove);
        }
    } else {
        remove = true;
    }
    if (remove) {
        log("delete _", *k);
        delete k;
    }
}

void sbn::connection::clear() {
    std::vector<std::unique_ptr<kernel>> sack;
    for (auto* k : this->_upstream) { k->mark_as_deleted(sack); }
    this->_upstream.clear();
    for (auto* k : this->_downstream) { k->mark_as_deleted(sack); }
    this->_downstream.clear();
}
