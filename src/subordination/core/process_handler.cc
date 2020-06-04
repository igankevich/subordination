#include <ostream>

#include <unistdx/base/make_object>

#include <subordination/core/basic_factory.hh>
#include <subordination/core/process_handler.hh>

/// Called from parent process.
sbn::process_handler::process_handler(sys::pid_type&& child,
                                      sys::two_way_pipe&& pipe,
                                      const application& app):
_childpid(child),
_buffer(new kernelbuf_type),
_stream(_buffer.get()),
_application(app),
_role(role_type::parent) {
    this->_buffer->setfd(sys::fildes_pair(std::move(pipe)));
    this->_buffer->fd().in().validate();
    this->_buffer->fd().out().validate();
}

/// Called from child process.
sbn::process_handler::process_handler(sys::pipe&& pipe):
_childpid(sys::this_process::id()),
_buffer(new kernelbuf_type),
_stream(_buffer.get()),
_application(),
_role(role_type::child) {
    this->_buffer->setfd(sys::fildes_pair(std::move(pipe)));
}

void sbn::process_handler::handle(const sys::epoll_event& event) {
    if (this->is_starting()) {
        this->setstate(pipeline_state::started);
    }
    if (event.in()) {
        this->_buffer->pubfill();
        if (this->_buffer->is_safe_to_compact()) {
            this->_buffer->compact();
        }
        this->_protocol->receive_kernels(this->_stream,
                                         sys::socket_address{},
                                         this->_role == role_type::parent
                                         ?  &this->_application : nullptr);
    }
}

void sbn::process_handler::write(std::ostream& out) const {
    out << sys::make_object(
        "pid",
        this->_childpid,
        "app",
        this->_application.id(),
        "in",
        this->in(),
        "out",
        this->out(),
        "remaining",
        this->_buffer->remaining(),
        "available",
        this->_buffer->available()
        );
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
