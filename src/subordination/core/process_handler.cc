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
_role(role_type::parent) {
    this->_file_descriptors.in().validate();
    this->_file_descriptors.out().validate();
}

/// Called from child process.
sbn::process_handler::process_handler(sys::pipe&& pipe):
_child_process_id(sys::this_process::id()),
_file_descriptors(std::move(pipe)),
_role(role_type::child) {}

void sbn::process_handler::handle(const sys::epoll_event& event) {
    if (this->is_starting()) {
        this->setstate(pipeline_state::started);
    }
    if (event.in()) {
        fill(this->_file_descriptors.in());
        receive_kernels(this->_role == role_type::parent ?  &this->_application : nullptr);
    }
}

void sbn::process_handler::remove(sys::event_poller& poller) {
    try {
        poller.erase(this->in());
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::no_such_file_or_directory) {
            throw;
        }
    }
    try {
        poller.erase(this->out());
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::no_such_file_or_directory) {
            throw;
        }
    }
    this->setstate(pipeline_state::stopped);
}

void sbn::process_handler::flush() {
    connection::flush(this->_file_descriptors.out());
}
