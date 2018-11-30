#include "child_process_pipeline.hh"

#include <bscheduler/config.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_router.hh>

namespace bsc {

	/*
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
			std::for_each(
				queue_popper(this->_ppl._kernels),
				queue_popper(),
				[this] (kernel_type* rhs) {
				    this->process_kernel(rhs);
				}
			);
		}

		void
		process_kernel(kernel_type* k) {
			if (this->_ppl._parent && this->_ppl._parent->is_running()) {
				this->_ppl._parent->send(k);
			} else {
				router_type::send_local(k);
			}
		}

	};

	*/

}

template <class K, class R>
bsc::child_process_pipeline<K,R>
::child_process_pipeline() {
	using namespace std::chrono;
	this->set_start_timeout(seconds(7));
	this->set_name("chld");
//	this->emplace_notify_handler(
//		std::make_shared<child_notify_handler<K,R>>(*this)
//	);
	sys::fd_type in = this_application::get_input_fd();
	sys::fd_type out = this_application::get_output_fd();
	if (in != -1 && out != -1) {
		this->_parent =
			std::make_shared<event_handler_type>(sys::pipe(in, out));
		this->_parent->setstate(pipeline_state::starting);
		this->_parent->set_name(this->name());
		try {
			this->emplace_handler(
				sys::epoll_event(in, sys::event::in),
				this->_parent
			);
		} catch (const sys::bad_call& err) {
			std::clog << err << std::endl;
			std::clog << "in=" << in << std::endl;
		}
		try {
			this->emplace_handler(
				sys::epoll_event(out, sys::event::out),
				this->_parent
			);
		} catch (const sys::bad_call& err) {
			std::clog << err << std::endl;
			std::clog << "out=" << out << std::endl;
		}
	}
}

template <class K, class R>
void
bsc::child_process_pipeline<K,R>
::process_kernels() {
	std::for_each(
		queue_popper(this->_kernels),
		queue_popper(),
		[this] (kernel_type* rhs) {
			this->process_kernel(rhs);
		}
	);
}

template <class K, class R>
void
bsc::child_process_pipeline<K,R>
::print_state(std::ostream& out) {
}

template class bsc::child_process_pipeline<
		BSCHEDULER_KERNEL_TYPE,
		bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
