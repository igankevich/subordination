#ifndef FACTORY_CPU_SERVER_HH
#define FACTORY_CPU_SERVER_HH

#include <unistdx/base/unlock_guard>
#include <unistdx/util/system>

#include <factory/error.hh>
#include <factory/ppl/basic_server.hh>
#include "basic_cpu_server.hh"

namespace factory {

	template<class T>
	class CPU_server: public Basic_CPU_server<T> {

	public:
		typedef Basic_CPU_server<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;
		typedef std::vector<base_server> server_pool;

		inline
		CPU_server(CPU_server&& rhs) noexcept:
		base_server(std::move(rhs)),
		_servers(std::move(rhs._servers))
		{}

		inline
		CPU_server() noexcept:
		CPU_server(sys::thread_concurrency())
		{}

		inline explicit
		CPU_server(unsigned concurrency) noexcept:
		base_server(concurrency),
		_servers(concurrency + priority_server)
		{
			this->set_name("upstrm");
			unsigned num = 0;
			for (base_server& srv : this->_servers) {
				srv.set_name("dwnstrm");
				srv.set_number(num);
				++num;
			}
		}

		CPU_server(const CPU_server&) = delete;
		CPU_server& operator=(const CPU_server&) = delete;
		~CPU_server() = default;

		inline void
		send(kernel_type* kernel) {
			#if defined(FACTORY_PRIORITY_SCHEDULING)
			if (kernel->isset(kernel_type::Flag::priority_service)) {
				this->_servers.back().send(kernel);
			} else
			#endif
			if (kernel->moves_downstream()) {
				const size_t i = kernel->hash();
				const size_t n = this->_servers.size()-priority_server;
				this->_servers[i%n].send(kernel);
			} else {
				base_server::send(kernel);
			}
		}

		void
		start();

		void
		stop();

		void
		wait();

	private:

		server_pool _servers;
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		static constexpr const size_t
		priority_server = 1;
		#else
		static constexpr const size_t
		priority_server = 0;
		#endif

	};

}

#endif // FACTORY_CPU_SERVER_HH
