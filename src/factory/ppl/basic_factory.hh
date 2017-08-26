#ifndef FACTORY_PPL_FACTORY_HH
#define FACTORY_PPL_FACTORY_HH

#if !defined(FACTORY_DAEMON) && !defined(FACTORY_APPLICATION)
#define FACTORY_DAEMON
#endif

#include <factory/config.hh>
#include <factory/ppl/cpu_server.hh>
#include <factory/ppl/basic_server.hh>
#include <factory/ppl/io_server.hh>
#include <factory/ppl/multi_pipeline.hh>
#if defined(FACTORY_DAEMON)
#include <factory/ppl/nic_server.hh>
#endif
#include <factory/ppl/process_server.hh>
#include <factory/ppl/timer_server.hh>
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
		typedef CPU_server<T> cpu_server;
		typedef Timer_server<T> timer_server;
		typedef IO_server<T> io_server;
		typedef Multi_pipeline<T> downstream_server;
		#if defined(FACTORY_APPLICATION)
		typedef Process_child_server<T, Basic_router<T>> parent_server;
		#endif
		#if defined(FACTORY_DAEMON)
		typedef NIC_server<T, sys::socket, Basic_router<T>> parent_server;
		#endif
		#if defined(FACTORY_DAEMON)
		typedef Process_iserver<T, Basic_router<T>> child_server;
		#endif

	private:
		cpu_server _upstream;
		downstream_server _downstream;
		timer_server _timer;
		io_server _io;
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		cpu_server _prio;
		#endif
		parent_server _parent;
		#if defined(FACTORY_DAEMON)
		child_server _child;
		#endif

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
			this->_parent.send(kernel);
		}

		inline void
		send_timer(kernel_type* kernel) {
			this->_timer.send(kernel);
		}

		inline void
		send_timer(kernel_type** kernel, size_t n) {
			this->_timer.send(kernel, n);
		}

		#if defined(FACTORY_DAEMON)
		inline void
		send_child(kernel_type* kernel) {
			this->_child.send(kernel);
		}

		inline child_server&
		child() noexcept {
			return this->_child;
		}

		inline const child_server&
		child() const noexcept {
			return this->_child;
		}
		#endif

		inline parent_server&
		parent() noexcept {
			return this->_parent;
		}

		inline const parent_server&
		parent() const noexcept {
			return this->_parent;
		}

		#if defined(FACTORY_DAEMON)
		inline parent_server&
		nic() noexcept {
			return this->_parent;
		}

		inline const parent_server&
		nic() const noexcept {
			return this->_parent;
		}
		#endif

		inline timer_server&
		timer() noexcept {
			return this->_timer;
		}

		inline const timer_server&
		timer() const noexcept {
			return this->_timer;
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
