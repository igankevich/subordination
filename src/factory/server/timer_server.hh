#ifndef FACTORY_SERVER_TIMER_SERVER_HH
#define FACTORY_SERVER_TIMER_SERVER_HH

#include "intro.hh"

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
		using Timer_server_base = Fast_server_with_pool<T, Time_priority_pool<T>>;

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

			friend std::ostream&
			operator<<(std::ostream& out, const Timer_server& rhs) {
				return out << "timerserver";
			}

			void add_cpu(size_t) {}

		private:

			void
			do_run() override {
				while (!this->stopped()) {
					lock_type lock(this->_mutex);
					wait_until_kernel_arrives(lock);
					if (!this->stopped()) {
						kernel_type* kernel = stdx::front(this->_kernels);
						if (!wait_until_kernel_is_ready(lock, kernel)) {
							stdx::pop(this->_kernels);
							lock.unlock();
							kernel->run_act();
						}
					}
				}
			}

			inline void
			wait_until_kernel_arrives(lock_type& lock) {
				this->_semaphore.wait(lock, [this] () {
					return this->stopped() || !this->_kernels.empty();
				});
			}

			inline bool
			wait_until_kernel_is_ready(lock_type& lock, kernel_type* kernel) {
				return this->_semaphore.wait_until(lock, kernel->at(),
					[this] { return this->stopped(); });
			}
			
		};

	}

	namespace stdx {

		template<class T>
		struct type_traits<components::Timer_server<T>> {
			static constexpr const char*
			short_name() { return "tm_server"; }
			typedef components::server_category category;
		};
	
	}

}
#endif // FACTORY_SERVER_TIMER_SERVER_HH
