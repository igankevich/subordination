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
	if (event.fd() == this->_outbuf->fd()) {
		this->_ostream.sync();
		if (this->_outbuf->dirty()) {
			event.setev(sys::poll_event::Out);
		} else {
			event.unsetev(sys::poll_event::Out);
		}
	} else {
		assert(
			event.fd() == this->_inbuf->fd()
			|| !this->_inbuf->fd()
			|| !event.bad_fd()
		);
		assert(!event.out() || event.hup());
		this->_istream.sync();
		this->_proto.receive_kernels(this->_istream);
	}
}

template class bsc::process_handler<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
