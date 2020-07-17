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
    find_kernel(sbn::kernel_ptr& a, const Queue& queue) {
        return std::find_if(queue.begin(), queue.end(),
                            [&a] (const sbn::kernel_ptr& b) {
                                return a->id() == b->id() &&
                                    a->source_application_id() == b->target_application_id();
                            });
    }

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

void sbn::connection::send(kernel_ptr& k) {
    // return local downstream kernels immediately
    // TODO we need to move some kernel flags to
    // kernel header in order to use them in routing
    if (k->phase() == kernel::phases::downstream &&
        !k->destination() &&
        k->target_application_id() == this_application::id()) {
        if (k->isset(kernel_flag::parent_is_id) || k->carries_parent()) {
            if (k->carries_parent()) {
                delete k->parent();
            }
            this->plug_parent(k);
        }
        #if defined(SBN_DEBUG)
        this->log("send local kernel _", *k);
        #endif
        this->parent()->native_pipeline()->send(std::move(k));
        return;
    }
    if (k->phase() == kernel::phases::upstream ||
        k->phase() == kernel::phases::point_to_point) {
        ensure_has_id(k->parent());
        ensure_has_id(k.get());
    }
    #if defined(SBN_DEBUG)
    this->log("send _ to _", *k, this->_socket_address);
    #endif
    this->write_kernel(k);
    /// The kernel is deleted if it goes downstream
    /// and does not carry its parent.
    k = save_kernel(std::move(k));
    if (k) {
        if (k->phase() == kernel::phases::downstream && k->carries_parent()) {
            delete k->parent();
        }
    }
}

sbn::foreign_kernel_ptr sbn::connection::do_forward(foreign_kernel_ptr k) {
    {
        kernel_frame frame;
        kernel_write_guard g(frame, this->_output_buffer);
        this->_output_buffer.write(k.get());
    }
    return pointer_dynamic_cast<foreign_kernel>(save_kernel(std::move(k)));
}

void sbn::connection::write_kernel(const kernel_ptr& k) noexcept {
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
                                      std::function<void(kernel_ptr&)> func) {
    kernel_frame frame;
    while (this->_input_buffer.remaining() >= sizeof(kernel_frame)) {
        try {
            kernel_read_guard g(frame, this->_input_buffer);
            if (!g) { break; }
            auto k = read_kernel(from_application);
            if (k->is_foreign()) {
                #if defined(SBN_DEBUG)
                log("read foreign src _ dst _ app _ id _", k->source(),
                    k->destination(), k->source_application_id(), k->id());
                #endif
                receive_foreign_kernel(pointer_dynamic_cast<foreign_kernel>(std::move(k)));
            } else {
                #if defined(SBN_DEBUG)
                log("read native src _ dst _ app _ id _", k->source(),
                    k->destination(), k->source_application_id(), k->id());
                #endif
                receive_kernel(std::move(k), func);
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

void sbn::connection::receive_foreign_kernel(foreign_kernel_ptr&& fk) {
    parent()->forward_foreign(std::move(fk));
}

sbn::kernel_ptr
sbn::connection::read_kernel(const application* from_application) {
    kernel_ptr k;
    this->_input_buffer.read(k);
    if (from_application) {
        k->source_application(new application(*from_application));
        if (k->is_foreign() && !k->target_application()) {
            k->target_application_id(from_application->id());
        }
    }
    if (this->_socket_address) { k->source(this->_socket_address); }
    return k;
}

void sbn::connection::receive_kernel(kernel_ptr&& k, std::function<void(kernel_ptr&)> func) {
    bool ok = true;
    if (k->phase() == kernel::phases::downstream) {
        this->plug_parent(k);
    } else if (k->principal_id()) {
        if (parent() && parent()->instances()) {
            auto& instances = *parent()->instances();
            auto g = instances.guard();
            auto result = instances.find(k->principal_id());
            if (result == instances.end()) {
                k->return_code(exit_code::no_principal_found);
                ok = false;
            }
            k->principal(result->second);
        }
    }
    #if defined(SBN_DEBUG)
    this->log("recv _", *k);
    #endif
    func(k);
    if (!ok) {
        #if defined(SBN_DEBUG)
        this->log("no principal found for _", *k);
        #endif
        //k->principal(k->parent());
        k->return_to_parent();
        this->send(k);
    } else {
        this->_parent->native_pipeline()->send(std::move(k));
    }
}

void sbn::connection::plug_parent(kernel_ptr& k) {
    if (k->type_id() == 1) {
        log("RECV main kernel _", *k);
        //return;
    }
    if (!k->has_id()) {
        throw std::invalid_argument("downstream kernel without an id");
    }
    auto result = find_kernel(k, this->_upstream);
    if (result == this->_upstream.end()) {
        if (k->carries_parent()) {
            k->principal(k->parent());
            this->log("use carried parent for _", *k);
            auto result2 = find_kernel(k, this->_downstream);
            if (result2 != this->_downstream.end()) {
                auto& old = *result2;
                this->log("delete _", *old);
                delete old->parent();
                this->_downstream.erase(result2);
            }
        } else if (k->type_id() != 1) {
            this->log("parent not found for _", *k);
            throw std::invalid_argument("parent not found");
        }
    } else {
        if (k->carries_parent()) {
            delete k->parent();
        }
        auto& orig = *result;
        #if defined(SBN_DEBUG)
        this->log("orig _", *orig);
        #endif
        k->parent(orig->parent());
        k->principal(k->parent());
        this->_upstream.erase(result);
        #if defined(SBN_DEBUG)
        this->log("plug parent for _", *k);
        #endif
    }
}

sbn::kernel_ptr sbn::connection::save_kernel(kernel_ptr k) {
    //if (k->type_id() == 1) {
    //    log("delete main kernel _", *k);
    //    return true;
    //}
    transaction_status status{};
    kernel_queue* queue{};
    switch (k->phase()) {
        case kernel::phases::upstream:
        case kernel::phases::point_to_point:
            if (isset(connection_flags::save_upstream_kernels)) {
                queue = &this->_upstream;
                if (k->carries_parent() || k->isset(kernel_flag::transactional)) {
                    status = transaction_status::start;
                }
            }
            break;
        case kernel::phases::downstream:
            if (isset(connection_flags::save_downstream_kernels) && k->carries_parent()) {
                queue = &this->_downstream;
                status = transaction_status::end;
            }
            break;
        case kernel::phases::broadcast:
            break;
    }
    if (isset(connection_flags::write_transaction_log) &&
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
    if (queue) {
        #if defined(SBN_DEBUG)
        this->log("save parent for _", *k);
        #endif
        queue->emplace_back(std::move(k));
    }
    return std::move(k);
}

void sbn::connection::recover_kernels(bool down) {
    kernel_queue* queues[2] { &this->_upstream, &this->_downstream };
    const int n = down ? 2 : 1;
    for (int i=0; i<n; ++i) {
        auto& queue = *queues[i];
        while (!queue.empty()) {
            auto k = std::move(queue.front());
            queue.pop_front();
            try {
                recover_kernel(k);
            } catch (const std::exception& err) {
                log("failed to recover kernel: _", err.what());
            }
        }
    }
}

void sbn::connection::recover_kernel(kernel_ptr& k) {
    if (parent() && (parent()->stopping() || parent()->stopped())) {
        log("trash _", *k);
        parent()->trash(std::move(k));
        return;
    }
    #if defined(SBN_DEBUG)
    this->log("try to recover kernel _ sent to _", k->id(), socket_address());
    #endif
    const bool native = k->is_native();
    if (k->phase() == kernel::phases::upstream && !k->destination()) {
        #if defined(SBN_DEBUG)
        this->log("recover _", *k);
        #endif
        if (native) {
            parent()->send_remote(std::move(k));
        } else {
            parent()->forward_remote(pointer_dynamic_cast<foreign_kernel>(std::move(k)));
        }
    } else if (k->phase() == kernel::phases::point_to_point ||
               (k->phase() == kernel::phases::upstream && k->destination())) {
        #if defined(SBN_DEBUG)
        this->log("destination is unreachable for _", *k);
        #endif
        /*
        k->phase(kernel::phases::downstream);
        k->source(k->destination());
        k->return_code(exit_code::endpoint_not_connected);
        k->principal(k->parent());
        */
        k->return_to_parent(exit_code::endpoint_not_connected);
        if (native) {
            parent()->send_native(std::move(k));
        } else {
            parent()->forward_foreign(pointer_dynamic_cast<foreign_kernel>(std::move(k)));
        }
    } else if (k->phase() == kernel::phases::downstream && k->carries_parent()) {
        #if defined(SBN_DEBUG)
        this->log("restore parent _", *k);
        #endif
        if (native) {
            parent()->send_native(std::move(k));
        } else {
            parent()->forward_foreign(pointer_dynamic_cast<foreign_kernel>(std::move(k)));
        }
    }
}

void sbn::connection::clear(kernel_sack& sack) {
    for (auto& k : this->_upstream) { k->mark_as_deleted(sack); k.release(); }
    this->_upstream.clear();
    for (auto& k : this->_downstream) { k->mark_as_deleted(sack); k.release(); }
    this->_downstream.clear();
}
