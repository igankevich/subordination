#ifndef FACTORY_PPL_FACTORY_HH
#define FACTORY_PPL_FACTORY_HH

#include <factory/ppl/basic_server.hh>
#include <factory/ppl/basic_cpu_server.hh>
#include <factory/ppl/io_server.hh>
#include <factory/ppl/nic_server.hh>
#include <factory/ppl/timer_server.hh>
#include <factory/ppl/multi_pipeline.hh>
#include <factory/config.hh>
#include <vector>

namespace factory {

	template <class T>
	struct Basic_router {

		inline void
		send_local(T* rhs);

		inline void
		send_remote(T*);

		inline void
		forward(const Kernel_header& hdr, sys::pstream& istr) {}

	};

	template <class T>
	class Factory: public Server_base {

	public:
		typedef T kernel_type;
		typedef Basic_CPU_server<T> cpu_server;
		typedef Timer_server<T> timer_server;
		typedef IO_server<T> io_server;
		typedef Multi_pipeline<T> downstream_server;
		typedef NIC_server<T, sys::socket, Basic_router<T>> nic_server;

	private:
		cpu_server _upstream;
		downstream_server _downstream;
		timer_server _timer;
		io_server _io;
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		cpu_server _prio;
		#endif
		nic_server _nic;

	public:
		Factory();
		virtual ~Factory() = default;
		Factory(const Factory&) = delete;
		Factory(Factory&&) = delete;

		inline void
		send(kernel_type* kernel) {
			if (kernel->scheduled()) {
				this->_timer.send(kernel);
			} else
			#if defined(FACTORY_PRIORITY_SCHEDULING)
			if (kernel->isset(kernel_type::Flag::priority_service)) {
				this->_prio.send(kernel);
			} else
			#endif
			if (kernel->moves_downstream()) {
				const size_t i = kernel->hash();
				const size_t n = this->_downstream.size();
				this->_downstream[i%n].send(kernel);
			} else {
				this->_upstream.send(kernel);
			}
		}

		inline void
		send_remote(kernel_type* kernel) {
			this->_nic.send(kernel);
		}

		inline nic_server&
		nic() noexcept {
			return this->_nic;
		}

		inline const nic_server&
		nic() const noexcept {
			return this->_nic;
		}

		void
		start();

		void
		stop();

		void
		wait();

	};

	extern Factory<FACTORY_KERNEL_TYPE> factory;

	template <class T>
	void
	Basic_router<T>::send_local(T* rhs) {
		factory.send(rhs);
	}

	template <class T>
	void
	Basic_router<T>::send_remote(T* rhs) {
		factory.send_remote(rhs);
	}

}

#endif // FACTORY_PPL_FACTORY_HH vim:filetype=cpp
