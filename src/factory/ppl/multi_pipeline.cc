#include "multi_pipeline.hh"
#include "config.hh"

template <class T>
factory::Multi_pipeline<T>::Multi_pipeline(unsigned nservers):
_servers(nservers) {
	unsigned num = 0;
	for (base_server& srv : this->_servers) {
		srv.set_number(num);
		++num;
	}
}

template <class T>
void
factory::Multi_pipeline<T>::set_name(const char* rhs) {
	for (base_server& srv : this->_servers) {
		srv.set_name(rhs);
	}
}


template <class T>
void
factory::Multi_pipeline<T>::start() {
	this->setstate(server_state::starting);
	for (base_server& srv : this->_servers) {
		srv.start();
	}
	this->setstate(server_state::started);
}

template <class T>
void
factory::Multi_pipeline<T>::stop() {
	this->setstate(server_state::stopping);
	for (base_server& srv : this->_servers) {
		srv.stop();
	}
	this->setstate(server_state::stopped);
}

template <class T>
void
factory::Multi_pipeline<T>::wait() {
	for (base_server& srv : this->_servers) {
		srv.wait();
	}
}

template class factory::Multi_pipeline<FACTORY_KERNEL_TYPE>;

