#include <subordination/ppl/process_handler.hh>

#include <ostream>

#include <unistdx/base/make_object>

#include <subordination/config.hh>
#include <subordination/ppl/basic_router.hh>

template <class K, class R>
void
sbn::process_handler<K,R>
::handle(const sys::epoll_event& event) {
    if (this->is_starting()) {
        this->setstate(pipeline_state::started);
    }
    if (event.in()) {
        this->_packetbuf->pubfill();
        if (this->_packetbuf->is_safe_to_compact()) {
            this->_packetbuf->compact();
        }
        this->_proto.receive_kernels(this->_stream);
    }
}

template <class K, class R>
void
sbn::process_handler<K,R>
::write(std::ostream& out) const {
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

template <class K, class R>
void
sbn::process_handler<K,R>
::remove(sys::event_poller& poller) {
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

template class sbn::process_handler<
        SUBORDINATION_KERNEL_TYPE, sbn::basic_router<SUBORDINATION_KERNEL_TYPE>>;
