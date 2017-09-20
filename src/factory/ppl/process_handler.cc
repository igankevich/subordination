#include "process_handler.hh"

#include <factory/config.hh>
#include <factory/ppl/basic_router.hh>

template <class K, class R>
void
factory::process_handler<K,R>
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

template class factory::process_handler<
		FACTORY_KERNEL_TYPE, factory::basic_router<FACTORY_KERNEL_TYPE>>;
