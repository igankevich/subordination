#ifndef FACTORY_PPL_TIMER_SERVER_HH
#define FACTORY_PPL_TIMER_SERVER_HH

#include <factory/kernel/algorithm.hh>
#include <factory/ppl/basic_pipeline.hh>
#include <factory/ppl/compare_time.hh>
#include <unistdx/ipc/semaphore>

namespace factory {

	template<class T>
	using Time_priority_pool = std::priority_queue<T*, std::vector<T*>, Compare_time<T>>;

	template<class T>
	using Timer_pipeline_base = basic_pipeline<T, Time_priority_pool<T>,
		sys::priority_queue_traits<Time_priority_pool<T>>>;

	template<class T>
	struct timer_pipeline: public Timer_pipeline_base<T> {

		typedef Timer_pipeline_base<T> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::traits_type;

		inline
		timer_pipeline(timer_pipeline&& rhs) noexcept:
		base_pipeline(std::move(rhs))
		{}

		inline
		timer_pipeline() noexcept:
		base_pipeline(1u)
		{}

		timer_pipeline(const timer_pipeline&) = delete;
		timer_pipeline& operator=(const timer_pipeline&) = delete;
		~timer_pipeline() = default;

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

#endif // vim:filetype=cpp
