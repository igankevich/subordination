#ifndef FACTORY_PPL_FACTORY_HH
#define FACTORY_PPL_FACTORY_HH

#include <factory/ppl/basic_server.hh>
#include <factory/ppl/cpu_server.hh>
#include <factory/ppl/io_server.hh>
#include <factory/ppl/nic_server.hh>
#include <factory/ppl/server_guard.hh>
#include <factory/ppl/timer_server.hh>
#include <factory/ppl/multi_pipeline.hh>
#include <vector>

namespace factory {

	template <class T>
	class Factory: public Server_base {

	public:
		typedef T kernel_type;
		typedef Basic_CPU_server<T> cpu_server;
		typedef Timer_server<T> timer_server;
		typedef IO_server<T> io_server;
		typedef Multi_pipeline<T> downstream_server;

	private:
		cpu_server _upstream;
		downstream_server _downstream;
		timer_server _timer;
		io_server _io;
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		cpu_server _prio;
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

		void
		start();

		void
		stop();

		void
		wait();

	};


}

#endif // FACTORY_PPL_FACTORY_HH vim:filetype=cpp
