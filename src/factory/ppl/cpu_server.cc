#include "cpu_server.hh"
#include "config.hh"

template <class T>
void
factory::CPU_server<T>::start() {
	base_server::start();
	for (base_server& srv : this->_servers) {
		srv.start();
	}
}

template <class T>
void
factory::CPU_server<T>::stop() {
	base_server::stop();
	for (base_server& srv : this->_servers) {
		srv.stop();
	}
}

template <class T>
void
factory::CPU_server<T>::wait() {
	base_server::wait();
	for (base_server& srv : this->_servers) {
		srv.wait();
	}
}

template class factory::CPU_server<FACTORY_KERNEL_TYPE>;
