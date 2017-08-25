#include "basic_factory.hh"
#include <factory/config.hh>
#include <factory/ppl/server_guard.hh>

template <class T>
factory::Factory<T>::Factory():
_upstream(),
_downstream(_upstream.concurrency()) {
	this->_upstream.set_name("upstrm");
	this->_downstream.set_name("dwnstrm");
	this->_timer.set_name("tmr");
	this->_io.set_name("io");
}

template <class T>
void
factory::Factory<T>::start() {
	this->setstate(server_state::starting);
	start_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		, this->_prio
		#endif
	);
	this->setstate(server_state::started);
}

template <class T>
void
factory::Factory<T>::stop() {
	this->setstate(server_state::stopping);
	stop_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		, this->_prio
		#endif
	);
	this->setstate(server_state::stopped);
}

template <class T>
void
factory::Factory<T>::wait() {
	wait_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		, this->_prio
		#endif
	);
}

template class factory::Factory<FACTORY_KERNEL_TYPE>;
