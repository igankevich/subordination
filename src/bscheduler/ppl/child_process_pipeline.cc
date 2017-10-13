#include "child_process_pipeline.hh"

#include <bscheduler/config.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_router.hh>

namespace bsc {

	template <class K, class R>
	class child_notify_handler: public basic_handler {

	public:
		typedef K kernel_type;
		typedef R router_type;
		typedef child_process_pipeline<K,R> this_type;
		typedef typename this_type::queue_popper queue_popper;

	private:
		this_type& _ppl;

	public:

		explicit
		child_notify_handler(this_type& ppl):
		_ppl(ppl) {}

		void
		handle(const sys::epoll_event& ev) override {
			this->_ppl.process_kernels();
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) {
				    this->process_kernel(rhs);
				}
			);
		}

		void
		process_kernel(kernel_type* k) {
			if (this->_parent && this->_parent->is_running()) {
				this->_parent->send(k);
			} else {
				router_type::send_local(k);
			}
		}

	};

}

template <class K, class R>
bsc::child_process_pipeline<K,R>
::child_process_pipeline() {
	this->set_name("chld");
	sys::fd_type in = this_application::get_input_fd();
	sys::fd_type out = this_application::get_output_fd();
	if (in != -1 && out != -1) {
		this->_parent =
			std::make_shared<event_handler_type>(sys::pipe(in, out));
		this->emplace_handler(
			sys::epoll_event(in, sys::event::in),
			this->_parent
		);
		this->emplace_handler(
			sys::epoll_event(out, sys::event::out),
			this->_parent
		);
		this->_parent->set_name(this->name());
	}
}

template class bsc::child_process_pipeline<
		BSCHEDULER_KERNEL_TYPE,
		bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
