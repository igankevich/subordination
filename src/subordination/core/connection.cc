#include <subordination/core/application.hh>
#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/error.hh>
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

    inline void forward_or_delete(sbn::pipeline* ppl, sbn::kernel* k) {
        if (ppl) { ppl->forward(dynamic_cast<sbn::foreign_kernel*>(k)); }
        else { delete k; }
    };

}

const char* sbn::to_string(connection_state rhs) {
    switch (rhs) {
        case connection_state::initial: return "initial";
        case connection_state::starting: return "starting";
        case connection_state::started: return "started";
        case connection_state::stopping: return "stopping";
        case connection_state::stopped: return "stopped";
        case connection_state::inactive: return "inactive";
        default: return "unknown";
    }
}

std::ostream& sbn::operator<<(std::ostream& out, connection_state rhs) {
    return out << to_string(rhs);
}

void sbn::connection::handle(const sys::epoll_event& event) {
    constexpr const size_t n = 20;
    char tmp[n];
    ssize_t c;
    while ((c = ::read(event.fd(), tmp, n)) != -1) ;
}

void sbn::connection::add(const connection_ptr& self) {}
void sbn::connection::remove(const connection_ptr& self) {}
void sbn::connection::retry(const connection_ptr& self) { ++this->_attempts; }
void sbn::connection::deactivate(const connection_ptr& self) { ++this->_attempts; }
void sbn::connection::activate(const connection_ptr& self) {}
void sbn::connection::flush() {}
void sbn::connection::stop() { state(connection_state::stopped); }

void sbn::connection::send(kernel* k) {
    // return local downstream kernels immediately
    // TODO we need to move some kernel flags to
    // kernel header in order to use them in routing
    if (k->moves_downstream() && !k->destination()) {
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

bool sbn::connection::do_forward(foreign_kernel* k) {
    bool delete_kernel = this->save_kernel(k);
    {
        kernel_frame frame;
        kernel_write_guard g(frame, this->_output_buffer);
        this->_output_buffer.write(k);
    }
    return delete_kernel;
}

void sbn::connection::write_kernel(kernel* k) noexcept {
    try {
        kernel_frame frame;
        kernel_write_guard g(frame, this->_output_buffer);
        log("write _ src _ dst _ src-app _ dst-app _", k->is_native() ? "native" : "foreign",
            k->source(), k->destination(), k->source_application_id(),
            k->target_application_id());
        this->_output_buffer.write(k);
    } catch (const sbn::error& err) {
        log_write_error(err);
    } catch (const std::exception& err) {
        log_write_error(err.what());
    } catch (...) {
        log_write_error("<unknown>");
    }
}

void sbn::connection::receive_kernels(const application* from_application,
                                      std::function<void(kernel*)> func) {
    kernel_frame frame;
    while (this->_input_buffer.remaining() >= sizeof(kernel_frame)) {
        try {
            kernel_read_guard g(frame, this->_input_buffer);
            if (!g) { break; }
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
        } catch (const sbn::error& err) {
            log_read_error(err);
        } catch (const std::exception& err) {
            log_read_error(err.what());
        } catch (...) {
            log_read_error("<unknown>");
        }
    }
    this->_input_buffer.compact();
}

sbn::kernel*
sbn::connection::read_kernel(const application* from_application) {
    kernel* k = nullptr;
    this->_input_buffer.read(k);
    if (from_application) {
        k->source_application(new application(*from_application));
        if (k->is_foreign() && !k->target_application()) {
            k->target_application_id(from_application->id());
        }
    }
    if (this->_socket_address) { k->source(this->_socket_address); }
    if (k->is_foreign()) {
        #if defined(SBN_DEBUG)
        this->log("read foreign src _ dst _ app _", k->source(),
                  k->destination(), k->source_application_id());
        #endif
        forward_or_delete(this->parent()->foreign_pipeline(), k);
        k = nullptr;
    } else {
        #if defined(SBN_DEBUG)
        this->log("read native src _ dst _ app _", k->source(),
                  k->destination(), k->source_application_id());
        #endif
    }
    return k;
}

bool sbn::connection::receive_kernel(kernel* k) {
    bool ok = true;
    if (k->moves_downstream()) {
        this->plug_parent(k);
    } else if (k->principal_id()) {
        if (parent() && parent()->instances()) {
            auto& instances = *parent()->instances();
            kernel_instance_registry::sentry s(instances);
            auto result = instances.find(k->principal_id());
            if (result == instances.end()) {
                k->return_code(exit_code::no_principal_found);
                ok = false;
            }
            k->principal(result->second);
        }
    }
    #if defined(SBN_DEBUG)
    this->log("recv _", k);
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
    transaction_status status{};
    if (bool(this->_flags & connection_flags::save_upstream_kernels) &&
        (k->moves_upstream() || k->moves_somewhere())) {
        if (k->is_native()) {
            this->ensure_has_id(k->parent());
            this->ensure_has_id(k);
        }
        #if defined(SBN_DEBUG)
        this->log("save parent for _", *k);
        #endif
        this->_upstream.push_back(k);
        status = transaction_status::start;
    } else if (bool(this->_flags & connection_flags::save_downstream_kernels) &&
               k->moves_downstream() && k->carries_parent()) {
        #if defined(SBN_DEBUG)
        this->log("save parent for _", *k);
        #endif
        this->_downstream.push_back(k);
        status = transaction_status::end;
    } else if (!k->moves_everywhere()) {
        delete_kernel = true;
    }
    if (bool(this->_flags & connection_flags::write_transaction_log) &&
        status != transaction_status{} && parent()->transactions()) {
        try {
            parent()->write_transaction(status, k);
        } catch (const sbn::error& err) {
            log_write_error(err);
        } catch (const std::exception& err) {
            log_write_error(err.what());
        } catch (...) {
            log_write_error("<unknown>");
        }
    }
    return delete_kernel;
}

void sbn::connection::recover_kernels(bool down) {
    #if defined(SBN_DEBUG)
    if (!this->_upstream.empty()) {
        log("recover _ upstream kernels", this->_upstream.size());
    }
    if (!this->_downstream.empty()) {
        log("recover _ downstream kernels", this->_downstream.size());
    }
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
    if (k->moves_upstream() && !k->destination()) {
        #if defined(SBN_DEBUG)
        this->log("recover _", *k);
        #endif
        if (native) {
            send_or_delete(this->parent()->remote_pipeline(), k, remove);
        } else {
            forward_or_delete(this->parent()->remote_pipeline(), k, remove);
        }
    } else if (k->moves_somewhere() || (k->moves_upstream() && k->destination())) {
        #if defined(SBN_DEBUG)
        this->log("destination is unreachable for _", *k);
        #endif
        k->source(k->destination());
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
