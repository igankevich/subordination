#ifndef FACTORY_CPU_SERVER_HH
#define FACTORY_CPU_SERVER_HH

#include <stdx/mutex.hh>
#include <sys/system.hh>
#include <factory/bits/server_category.hh>
#include <factory/basic_server.hh>
#include <factory/result.hh>
#include <factory/algorithm.hh>

namespace factory {

	template<class T>
	struct Basic_CPU_server: public Standard_server_with_pool<T> {

		typedef Standard_server_with_pool<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;
		typedef stdx::log<Basic_CPU_server> this_log;

		Basic_CPU_server(Basic_CPU_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		Basic_CPU_server() noexcept:
		Basic_CPU_server(1u)
		{}

		explicit
		Basic_CPU_server(unsigned concurrency) noexcept:
		base_server(concurrency)
		{}

		Basic_CPU_server(const Basic_CPU_server&) = delete;
		Basic_CPU_server& operator=(const Basic_CPU_server&) = delete;
		~Basic_CPU_server() = default;

	private:

		void
		do_run() override {
			lock_type lock(this->_mutex);
			this->_semaphore.wait(lock, [this,&lock] () {
				while (!this->_kernels.empty()) {
					kernel_type* kernel = stdx::front(this->_kernels);
					stdx::pop(this->_kernels);
					stdx::unlock_guard<lock_type> g(lock);
					try {
						::factory::act(kernel);
					} catch (std::exception& x) {
						throw Error(x.what(), {__FILE__, __LINE__, __func__});
					}
				}
				return this->is_stopped();
			});
		}

	};

	template<class T>
	struct CPU_server: public Basic_CPU_server<T> {

		typedef Basic_CPU_server<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;
		typedef std::vector<base_server> server_pool;

		CPU_server(CPU_server&& rhs) noexcept:
		base_server(std::move(rhs)),
		_servers(std::move(rhs._servers))
		{}

		CPU_server() noexcept:
		CPU_server(sys::thread_concurrency())
		{}

		explicit
		CPU_server(unsigned concurrency) noexcept:
		base_server(concurrency),
		_servers(concurrency + priority_server)
		{}

		CPU_server(const CPU_server&) = delete;
		CPU_server& operator=(const CPU_server&) = delete;
		~CPU_server() = default;

		void
		send(kernel_type* kernel) {
			#if defined(FACTORY_PRIORITY_SCHEDULING)
			if (kernel->isset(kernel_type::Flag::priority_service)) {
				_servers.back().send(kernel);
			} else
			#endif
			if (kernel->moves_downstream()) {
				size_t i = kernel->hash();
				_servers[i % (_servers.size()-priority_server)].send(kernel);
			} else {
				base_server::send(kernel);
			}
		}

		void
		start() {
			base_server::start();
			std::for_each(
				_servers.begin(),
				_servers.end(),
				[this] (base_server& rhs) {
					// rhs.setparent(this);
					rhs.start();
				}
			);
		}

		void
		stop() {
			base_server::stop();
			std::for_each(
				_servers.begin(),
				_servers.end(),
				std::mem_fn(&server_pool::value_type::stop)
			);
		}

		void
		wait() {
			base_server::wait();
			std::for_each(
				_servers.begin(),
				_servers.end(),
				std::mem_fn(&server_pool::value_type::wait)
			);
		}

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

namespace stdx {

	template<class T>
	struct type_traits<factory::CPU_server<T>> {
		static constexpr const char*
		short_name() { return "cpu_server"; }
		typedef factory::server_category category;
	};

}

#endif // FACTORY_CPU_SERVER_HH
