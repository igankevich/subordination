#ifndef BSCHEDULER_PPL_TIMER_PIPELINE_HH
#define BSCHEDULER_PPL_TIMER_PIPELINE_HH

#include <bscheduler/kernel/act.hh>
#include <bscheduler/ppl/basic_pipeline.hh>
#include <bscheduler/ppl/compare_time.hh>
#include <unistdx/ipc/semaphore>

namespace bsc {

	namespace bits {

		template<class T>
		using Priority_queue =
				  std::priority_queue<T*, std::vector<T*>, Compare_time<T>>;

		template<class T>
		using Priority_queue_traits =
				  sys::priority_queue_traits<Priority_queue<T>>;

		template<class T>
		using Timer_pipeline_base =
				  basic_pipeline<T, Priority_queue<T>,
				                 Priority_queue_traits<T>>;

	}

	template<class T>
	struct timer_pipeline: public bits::Timer_pipeline_base<T> {

		typedef bits::Timer_pipeline_base<T> base_pipeline;
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

		timer_pipeline&
		operator=(const timer_pipeline&) = delete;

		~timer_pipeline() = default;

	protected:
		void
		do_run() override;

	private:
		inline void
		wait_until_kernel_arrives(lock_type& lock) {
			this->_semaphore.wait(
				lock,
				[this] () {
				    return this->is_stopped() || !this->_kernels.empty();
				}
			);
		}

		inline bool
		wait_until_kernel_is_ready(lock_type& lock, kernel_type* k) {
			return this->_semaphore.wait_until(
				lock,
				k->at(),
				[this] { return this->is_stopped(); }
			);
		}

	};

}

#endif // vim:filetype=cpp
