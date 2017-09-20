#include "basic_factory.hh"
#include <factory/config.hh>

namespace {

	inline void
	start_all() {}

	template <class Pipeline, class ... Args>
	void
	start_all(Pipeline& ppl, Args& ... args) {
		ppl.start();
		start_all(args...);
	}

	inline void
	stop_all() {}

	template <class Pipeline, class ... Args>
	void
	stop_all(Pipeline& ppl, Args& ... args) {
		ppl.stop();
		stop_all(args...);
	}

	inline void
	wait_all() {}

	template <class Pipeline, class ... Args>
	void
	wait_all(Pipeline& ppl, Args& ... args) {
		ppl.wait();
		wait_all(args...);
	}

}

template <class T>
factory::Factory<T>::Factory():
_upstream(std::thread::hardware_concurrency()),
_downstream(_upstream.concurrency()) {
	this->_upstream.set_name("upstrm");
	this->_downstream.set_name("dwnstrm");
	this->_timer.set_name("tmr");
	this->_io.set_name("io");
	#if defined(FACTORY_DAEMON)
	this->_parent.set_name("nic");
	#elif defined(FACTORY_SUBMIT)
	this->_parent.set_name("unix");
	#endif
	#if defined(FACTORY_DAEMON)
	this->_child.set_name("proc");
	this->_external.set_name("unix");
	#endif
}

template <class T>
void
factory::Factory<T>::start() {
	this->setstate(pipeline_state::starting);
	start_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		, this->_parent
		#if defined(FACTORY_DAEMON)
		, this->_child
		, this->_external
		#endif
	);
	this->setstate(pipeline_state::started);
}

template <class T>
void
factory::Factory<T>::stop() {
	this->setstate(pipeline_state::stopping);
	stop_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		, this->_parent
		#if defined(FACTORY_DAEMON)
		, this->_child
		, this->_external
		#endif
	);
	this->setstate(pipeline_state::stopped);
}

template <class T>
void
factory::Factory<T>::wait() {
	wait_all(
		this->_upstream,
		this->_downstream,
		this->_timer,
		this->_io
		, this->_parent
		#if defined(FACTORY_DAEMON)
		, this->_child
		, this->_external
		#endif
	);
}

template class factory::Factory<FACTORY_KERNEL_TYPE>;
template class factory::basic_router<FACTORY_KERNEL_TYPE>;

factory::Factory<FACTORY_KERNEL_TYPE> factory::factory;
