#include "process_handler.hh"

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_router.hh>

template <class K, class R>
void
bsc::process_handler<K,R>
::handle(sys::poll_event& event) {
	if (this->is_starting()) {
		this->setstate(pipeline_state::started);
	}
	this->log("_ _", __func__, event);
	if (event.fd() == this->_outbuf->fd()) {
		event.setev(sys::poll_event::Out);
		this->_ostream.sync();
	} else {
		this->_istream.sync();
		this->_proto.receive_kernels(this->_istream);
	}
//	if (this->_outbuf->dirty()) {
//		event.setev(sys::poll_event::Out);
//	} else {
//		event.unsetev(sys::poll_event::Out);
//	}
}

template class bsc::process_handler<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
