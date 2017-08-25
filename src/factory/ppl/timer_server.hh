#ifndef FACTORY_TIMER_SERVER_HH
#define FACTORY_TIMER_SERVER_HH

#include <factory/kernel/algorithm.hh>
#include <factory/ppl/basic_server.hh>
#include <factory/ppl/compare_time.hh>
#include <unistdx/ipc/semaphore>

namespace factory {

	template<class T>
	using Time_priority_pool = std::priority_queue<T*, std::vector<T*>, Compare_time<T>>;

	template<class T>
	using Timer_server_base = Basic_server<T, Time_priority_pool<T>,
		sys::priority_queue_traits<Time_priority_pool<T>>>;

	template<class T>
	struct Timer_server: public Timer_server_base<T> {

		typedef Timer_server_base<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::traits_type;

		inline
		Timer_server(Timer_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		inline
		Timer_server() noexcept:
		base_server(1u)
		{}

		Timer_server(const Timer_server&) = delete;
		Timer_server& operator=(const Timer_server&) = delete;
		~Timer_server() = default;

	protected:
		void
		do_run() override;

	private:
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

#endif // FACTORY_TIMER_SERVER_HH
