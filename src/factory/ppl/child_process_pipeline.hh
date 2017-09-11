#ifndef FACTORY_PPL_CHILD_PROCESS_PIPELINE_HH
#define FACTORY_PPL_CHILD_PROCESS_PIPELINE_HH

#include <algorithm>
#include <memory>

#include <unistdx/it/queue_popper>
#include <unistdx/io/pipe>

#include <factory/ppl/shared_fildes.hh>
#include <factory/ppl/process_handler.hh>

namespace factory {

	template<class T, class Router>
	class child_process_pipeline:
		public basic_socket_pipeline<T,process_handler<T,Router>> {

	public:
		typedef basic_socket_pipeline<T,process_handler<T,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		typedef Router router_type;

		using base_pipeline::poller;

		child_process_pipeline():
		_parent(std::make_shared<event_handler_type>(
			sys::pipe{Shared_fildes::In, Shared_fildes::Out}
		))
		{}

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual ~child_process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override {
			if (!this->is_stopped()) {
				this->stop();
				// this->factory()->stop();
			}
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
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

	private:

		void
		init_pipeline() {
			// _parent.setparent(this);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::In, sys::poll_event::In, 0},
				this->_parent
			);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::Out, 0, 0},
				this->_parent
			);
			this->_parent->set_name(this->name());
		}

		void
		process_kernel(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
			this->_parent->send(k);
		}

		event_handler_ptr _parent;
	};

}

#endif // vim:filetype=cpp
