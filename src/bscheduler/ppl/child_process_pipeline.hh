#ifndef BSCHEDULER_PPL_CHILD_PROCESS_PIPELINE_HH
#define BSCHEDULER_PPL_CHILD_PROCESS_PIPELINE_HH

#include <algorithm>
#include <memory>

#include <unistdx/io/pipe>
#include <unistdx/it/queue_popper>

#include <bscheduler/ppl/basic_socket_pipeline.hh>
#include <bscheduler/ppl/process_handler.hh>

namespace bsc {

	template<class K, class R>
	class child_process_pipeline: public basic_socket_pipeline<K> {

	private:
		typedef basic_socket_pipeline<K> base_pipeline;
		typedef process_handler<K,R> event_handler_type;
		typedef std::shared_ptr<event_handler_type> event_handler_ptr;

	public:
		typedef K kernel_type;
		typedef R router_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::queue_popper;
		using typename base_pipeline::traits_type;

	private:
		event_handler_ptr _parent;

	public:

		child_process_pipeline();

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual
		~child_process_pipeline() = default;

		void
		send(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
			lock_type lock(this->_mutex);
			if (!this->_parent) {
				lock.unlock();
				router_type::send_local(k);
			} else {
				traits_type::push(this->_kernels, k);
				this->poller().notify_one();
			}
		}

	private:

		template <class X, class Y> friend class child_notify_handler;
		template <class X, class Y> friend class process_handler;

	};

}

#endif // vim:filetype=cpp
