#include "process_handler.hh"

#include <ostream>

#include <unistdx/base/make_object>

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_router.hh>

template <class K, class R>
void
bsc::process_handler<K,R>
::handle(const sys::epoll_event& event) {
	if (this->is_starting()) {
		this->setstate(pipeline_state::started);
	}
	this->log("_ _", __func__, event);
	if (event.fd() == this->_outbuf->fd()) {
		this->_ostream.sync();
	} else {
		this->_istream.sync();
		this->_proto.receive_kernels(this->_istream);
	}
}

template <class K, class R>
void
bsc::process_handler<K,R>
::write(std::ostream& out) const {
	out << sys::make_object(
		"pid",
		this->_childpid,
		"app",
		this->_application.id(),
		"in",
		this->_inbuf->fd(),
		"out",
		this->_outbuf->fd()
	    );
}

template <class K, class R>
void
bsc::process_handler<K,R>
::remove(sys::event_poller& poller) {
	poller.erase(this->_inbuf->fd().get_fd());
	poller.erase(this->_outbuf->fd().get_fd());
}

template class bsc::process_handler<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
