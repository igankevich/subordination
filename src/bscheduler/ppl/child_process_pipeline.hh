#ifndef BSCHEDULER_PPL_CHILD_PROCESS_PIPELINE_HH
#define BSCHEDULER_PPL_CHILD_PROCESS_PIPELINE_HH

#include <algorithm>
#include <memory>

#include <unistdx/io/pipe>
#include <unistdx/it/queue_popper>

#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/process_handler.hh>

namespace bsc {

	template<class K, class R>
	class child_process_pipeline:
		public basic_socket_pipeline<K,process_handler<K,R>> {

	public:
		typedef basic_socket_pipeline<K,process_handler<K, R>>
		    base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		using typename base_pipeline::traits_type;
		typedef R router_type;

		child_process_pipeline():
		_parent(std::make_shared<event_handler_type>(
					sys::pipe {
					    this_application::get_input_fd(),
					    this_application::get_output_fd()
					}

		        ))
		{
			this->set_name("chld");
		}

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual
		~child_process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override {
			if (!this->has_stopped()) {
				this->xstop();
				this->poller().notify_one();
			}
		}

		void
		process_kernels() override {
//			lock_type lock(this->_mutex);
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

		void
		do_run() override {
			this->init_pipeline();
			base_pipeline::do_run();
		}

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
				this->_semaphore.notify_one();
			}
		}

	private:

		void
		init_pipeline() {
			sys::fd_type in = this_application::get_input_fd();
			sys::fd_type out = this_application::get_output_fd();
			if (in != -1 && out != -1) {
				this->poller().emplace(
					sys::poll_event {in, sys::poll_event::In, 0},
					this->_parent
				);
				this->poller().emplace(
					sys::poll_event {out, 0, 0},
					this->_parent
				);
				this->_parent->set_name(this->name());
			} else {
				this->_parent = nullptr;
			}
		}

		void
		process_kernel(kernel_type* k) {
			if (this->_parent) {
				this->_parent->send(k);
			} else {
				router_type::send_local(k);
			}
		}

		event_handler_ptr _parent;
	};

}

#endif // vim:filetype=cpp
