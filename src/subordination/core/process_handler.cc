#include <ostream>

#include <unistdx/base/make_object>

#include <subordination/core/basic_factory.hh>
#include <subordination/core/process_handler.hh>

/// Called from parent process.
sbn::process_handler::process_handler(sys::pid_type&& child,
                                      sys::two_way_pipe&& pipe,
                                      const ::sbn::application& app):
_child_process_id(child),
_file_descriptors(std::move(pipe)),
_application(app),
_role(roles::parent) {
    this->_file_descriptors.in().validate();
    this->_file_descriptors.out().validate();
}

/// Called from child process.
sbn::process_handler::process_handler(sys::pipe&& pipe):
_child_process_id(sys::this_process::id()),
_file_descriptors(std::move(pipe)),
_role(roles::child) {}

sbn::kernel_ptr sbn::process_handler::read_kernel() {
    auto k = connection::read_kernel();
    if (this->_role == roles::parent) {
        k->source_application(new sbn::application(this->_application));
        if (k->is_foreign() && !k->target_application()) {
            k->target_application_id(this->_application.id());
        }
    }
    return k;
}

void sbn::process_handler::write_kernel(const kernel* k) noexcept {
    connection::write_kernel(k);
    if (k->phase() == sbn::kernel::phases::upstream) {
        ++this->_num_active_kernels;
        this->_load += k->weights();
    }
}

void sbn::process_handler::handle(const sys::epoll_event& event) {
    if (state() == connection_state::starting) {
        state(connection_state::started);
    }
    if (event.in()) {
        fill(this->_file_descriptors.in());
        receive_kernels();
        log("recv DEBUG upstream _ downstream _", upstream().size(), downstream().size());
    }
}

void sbn::process_handler::receive_kernel(kernel_ptr&& k) {
    // TODO move this code to sbn::connection
    if (k->phase() == sbn::kernel::phases::downstream) {
        --this->_num_active_kernels;
        this->_load -= k->weights();
    }
    connection::receive_kernel(std::move(k));
}

void sbn::process_handler::receive_foreign_kernel(foreign_kernel_ptr&& fk) {
    if (fk->phase() == sbn::kernel::phases::downstream) {
        this->_load -= fk->weights();
    }
    if (fk->type_id() == 1) {
        log("RECV _", *fk);
        //fk->return_to_parent();
        //fk->principal_id(0);
        //fk->parent_id(0);
        //connection::forward(std::move(fk));
        fk->target_application_id(1);
        parent()->forward_to(this->_unix, std::move(fk));
        //parent()->remove(application().id());
        sys::process_view(child_process_id()).send(sys::signal::terminate);
    } else {
        connection::receive_foreign_kernel(std::move(fk));
    }
}

void sbn::process_handler::add(const connection_ptr& self) {
    connection::parent()->emplace_handler(sys::epoll_event(in(), sys::event::in), self);
    connection::parent()->emplace_handler(sys::epoll_event(out(), sys::event::out), self);
}

void sbn::process_handler::remove(const connection_ptr&) {
    try {
        connection::parent()->erase(in());
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::no_such_file_or_directory) { throw; }
    }
    try {
        connection::parent()->erase(out());
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::no_such_file_or_directory) { throw; }
    }
    state(connection_state::stopped);
}

void sbn::process_handler::flush() {
    connection::flush(this->_file_descriptors.out());
}

void sbn::process_handler::stop() {
}

void sbn::process_handler::forward(foreign_kernel_ptr&& k) {
    // remove target application before forwarding
    // to child process to reduce the amount of data
    // transferred to child process
    bool wait_for_completion = false;
    if (auto* a = k->target_application()) {
        wait_for_completion = a->wait_for_completion();
        if (k->source_application_id() == a->id()) {
            k->target_application_id(a->id());
        }
    }
    // save the main kernel
    k = connection::do_forward(std::move(k));
    if (k) {
        if (k->type_id() == 1) {
            if (wait_for_completion) {
                log("save main kernel _", *k);
                this->_main_kernel = std::move(k);
            } else {
                log("return main kernel _", *k);
                k->return_to_parent();
                k->target_application_id(0);
                k->source_application_id(application().id());
                parent()->forward_to(this->_unix, std::move(k));
            }
        }
    }
    log("send DEBUG upstream _ downstream _", upstream().size(), downstream().size());
}
