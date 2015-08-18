#ifndef FACTORY_SERVER_TIMER_SERVER_HH
#define FACTORY_SERVER_TIMER_SERVER_HH

namespace factory {

	namespace components {

		template<class T>
		struct Timer_server: public Timer_server_base<T> {

			typedef Timer_server_base<T> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			typedef unix::timer<sem_type> timer_type;

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

			void
			stop() override {
				base_server::stop();
				_timer.notify_all();
			}

		private:

			void
			do_run() override {
				while (!this->stopped()) {
					lock_type lock(this->_mutex);
					this->_semaphore.wait(lock, [this] () {
						return this->stopped() || !this->_kernels.empty();
					});
					if (!this->stopped()) {
						kernel_type* kernel = stdx::front(this->_kernels);
						stdx::pop(this->_kernels);
						lock.unlock();
						if (!wait_until(kernel)) {
							kernel->run_act();
						}
					}
				}
			}

			inline bool
			wait_until(kernel_type* kernel) {
				return _timer.wait_until(kernel->at(),
					[this] { return this->stopped(); });
			}
			
			timer_type _timer;
		};

	}

}
#endif // FACTORY_SERVER_TIMER_SERVER_HH
