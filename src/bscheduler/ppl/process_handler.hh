#ifndef BSCHEDULER_PPL_PROCESS_HANDLER_HH
#define BSCHEDULER_PPL_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

#include <unistdx/io/fildes_pair>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/ipc/process>

#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_handler.hh>
#include <bscheduler/ppl/kernel_protocol.hh>
#include <bscheduler/ppl/pipeline_base.hh>

namespace bsc {

	template<class K, class R>
	class process_handler: public basic_handler {

	public:
		typedef K kernel_type;
		typedef R router_type;

	private:
		typedef sys::basic_fildesbuf<char, std::char_traits<char>, sys::fildes_pair>
			fildesbuf_type;
		typedef basic_kernelbuf<fildesbuf_type> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<K> stream_type;
		typedef kernel_protocol<K,R,bits::forward_to_parent<R>>
			protocol_type;

		enum class role_type {
			child,
			parent
		};

	private:
		sys::pid_type _childpid;
		kernelbuf_ptr _packetbuf;
		stream_type _stream;
		protocol_type _proto;
		application _application;
		role_type _role;

	public:

		/// Called from parent process.
		process_handler(
			sys::pid_type&& child,
			sys::two_way_pipe&& pipe,
			const application& app
		):
		_childpid(child),
		_packetbuf(new kernelbuf_type),
		_stream(_packetbuf.get()),
		_proto(),
		_application(app),
		_role(role_type::parent)
		{
			this->_proto.set_other_application(&this->_application);
			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
			this->_packetbuf->setfd(sys::fildes_pair(std::move(pipe)));
			this->_packetbuf->fd().in().validate();
			this->_packetbuf->fd().out().validate();
		}

		/// Called from child process.
		explicit
		process_handler(sys::pipe&& pipe):
		_childpid(sys::this_process::id()),
		_packetbuf(new kernelbuf_type),
		_stream(_packetbuf.get()),
		_proto(),
		_application(),
		_role(role_type::child)
		{
			this->_proto.setf(
				kernel_proto_flag::prepend_source_and_destination |
				kernel_proto_flag::save_upstream_kernels
			);
			this->_packetbuf->setfd(sys::fildes_pair(std::move(pipe)));
		}

		virtual
		~process_handler() {
			// recover kernels from upstream and downstream buffer
			this->_proto.recover_kernels(true);
		}

		const sys::pid_type&
		childpid() const {
			return this->_childpid;
		}

		const application&
		app() const noexcept {
			return this->_application;
		}

		void
		close() {
			this->_packetbuf->fd().close();
		}

		void
		send(kernel_type* k) {
			this->_proto.send(k, this->_stream);
		}

		void
		handle(const sys::epoll_event& event) override;

		void
		flush() override {
			if (this->_packetbuf->dirty()) {
				this->_packetbuf->pubflush();
			}
		}

		void
		write(std::ostream& out) const override;

		void
		remove(sys::event_poller& poller) override;

		void
		forward(foreign_kernel* k) {
			// remove application before forwarding
			// to child process
			k->aptr(nullptr);
			this->_proto.forward(k, this->_stream);
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->pipeline_base::set_name(rhs);
			this->_proto.set_name(rhs);
			#ifndef NDEBUG
			if (this->_packetbuf) {
				this->_packetbuf->set_name(rhs);
			}
			#endif
		}

		inline sys::fd_type
		in() const noexcept {
			return this->_packetbuf->fd().in().get_fd();
		}

		inline sys::fd_type
		out() const noexcept {
			return this->_packetbuf->fd().out().get_fd();
		}

	};

}

#endif // vim:filetype=cpp
