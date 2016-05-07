#ifndef FACTORY_TIMER_SERVER_HH
#define FACTORY_TIMER_SERVER_HH

#include <sys/semaphore.hh>
#include <factory/bits/server_category.hh>
#include <factory/basic_server.hh>
#include <factory/algorithm.hh>

namespace factory {

	namespace components {

		template<class T>
		struct Compare_time {
			inline bool
			operator()(const T* lhs, const T* rhs) const noexcept {
				return lhs->at() > rhs->at();
			}
		};

		template<class T>
		using Time_priority_pool = std::priority_queue<T*, std::vector<T*>, Compare_time<T>>;

		template<class T>
		using Timer_server_base = Standard_server_with_pool<T, Time_priority_pool<T>>;

		template<class T>
		struct Timer_server: public Timer_server_base<T> {

			typedef Timer_server_base<T> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;

			Timer_server(Timer_server&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			Timer_server() noexcept:
			base_server(1u)
			{}

			Timer_server(const Timer_server&) = delete;
			Timer_server& operator=(const Timer_server&) = delete;
			~Timer_server() = default;

		private:

			void
			do_run() override {
				while (!this->is_stopped()) {
					lock_type lock(this->_mutex);
					wait_until_kernel_arrives(lock);
					if (!this->is_stopped()) {
						kernel_type* kernel = stdx::front(this->_kernels);
						if (!wait_until_kernel_is_ready(lock, kernel)) {
							stdx::pop(this->_kernels);
							lock.unlock();
							::factory::act(kernel);
						}
					}
				}
			}

			inline void
			wait_until_kernel_arrives(lock_type& lock) {
				this->_semaphore.wait(lock, [this] () {
					return this->is_stopped() || !this->_kernels.empty();
				});
			}

			inline bool
			wait_until_kernel_is_ready(lock_type& lock, kernel_type* kernel) {
				return this->_semaphore.wait_until(lock, kernel->at(),
					[this] { return this->is_stopped(); });
			}

		};

	}

}

namespace stdx {

	template<class T>
	struct type_traits<factory::components::Timer_server<T>> {
		static constexpr const char*
		short_name() { return "tm_server"; }
		typedef factory::components::server_category category;
	};

}

#endif // FACTORY_TIMER_SERVER_HH
