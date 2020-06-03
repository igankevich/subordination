#include <ostream>

#include <unistdx/base/make_object>

#include <subordination/ppl/basic_factory.hh>
#include <subordination/ppl/process_handler.hh>

/// Called from parent process.
sbn::process_handler::process_handler(sys::pid_type&& child,
                                      sys::two_way_pipe&& pipe,
                                      const application& app):
_childpid(child),
_packetbuf(new kernelbuf_type),
_stream(_packetbuf.get()),
_proto(),
_application(app),
_role(role_type::parent)
{
    this->_proto.set_other_application(&this->_application);
    this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
    this->_proto.foreign_pipeline(&factory.parent());
    this->_packetbuf->setfd(sys::fildes_pair(std::move(pipe)));
    this->_packetbuf->fd().in().validate();
    this->_packetbuf->fd().out().validate();
}

/// Called from child process.
sbn::process_handler::process_handler(sys::pipe&& pipe):
_childpid(sys::this_process::id()),
_packetbuf(new kernelbuf_type),
_stream(_packetbuf.get()),
_proto(),
_application(),
_role(role_type::child)
{
    this->_proto.setf(
        kernel_proto_flag::prepend_source_and_destination |
        kernel_proto_flag::save_upstream_kernels
    );
    this->_proto.foreign_pipeline(&factory.parent());
    this->_packetbuf->setfd(sys::fildes_pair(std::move(pipe)));
}

void sbn::process_handler::handle(const sys::epoll_event& event) {
    if (this->is_starting()) {
        this->setstate(pipeline_state::started);
    }
    if (event.in()) {
        this->_packetbuf->pubfill();
        if (this->_packetbuf->is_safe_to_compact()) {
            this->_packetbuf->compact();
        }
        this->_proto.receive_kernels(this->_stream, sys::socket_address{});
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
        this->_packetbuf->remaining(),
        "available",
        this->_packetbuf->available()
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
