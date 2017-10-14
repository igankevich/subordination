#ifndef BSCHEDULER_PPL_PROCESS_HANDLER_HH
#define BSCHEDULER_PPL_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

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
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
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
		kernelbuf_ptr _outbuf;
		stream_type _ostream;
		kernelbuf_ptr _inbuf;
		stream_type _istream;
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
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto(),
		_application(app),
		_role(role_type::parent)
		{
			this->_proto.set_other_application(&this->_application);
			this->_outbuf->setfd(std::move(pipe.parent_out()));
			this->_outbuf->fd().validate();
			this->_inbuf->setfd(std::move(pipe.parent_in()));
			this->_inbuf->fd().validate();
			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
		}

		/*
		process_handler(process_handler&& rhs):
		_childpid(rhs._childpid),
		_outbuf(std::move(rhs._outbuf)),
		_ostream(std::move(rhs._ostream)),
		_inbuf(std::move(rhs._inbuf)),
		_istream(std::move(rhs._istream)),
		_proto(std::move(rhs._proto)),
		_application(std::move(rhs._application)),
		_role(rhs._role)
		{
			this->_inbuf->fd().validate();
			this->_outbuf->fd().validate();
			this->_istream.rdbuf(this->_inbuf.get());
			this->_ostream.rdbuf(this->_outbuf.get());
		}
		*/

		/// Called from child process.
		explicit
		process_handler(sys::pipe&& pipe):
		_childpid(sys::this_process::id()),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto(),
		_application(),
		_role(role_type::child)
		{
			this->_outbuf->setfd(std::move(pipe.out()));
			this->_inbuf->setfd(std::move(pipe.in()));
			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
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
			this->_outbuf->fd().close();
			this->_inbuf->fd().close();
		}

		void
		send(kernel_type* k) {
			this->_proto.send(k, this->_ostream);
		}

		void
		handle(const sys::epoll_event& event) override;

		void
		flush() override {
			this->_outbuf->sync();
		}

		void
		write(std::ostream& out) const override;

		void
		remove(sys::event_poller& poller) override;

		void
		forward(kernel_header& hdr, sys::pstream& istr) {
			// remove application before forwarding
			// to child process
			hdr.aptr(nullptr);
			this->_proto.forward(hdr, istr, this->_ostream);
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->pipeline_base::set_name(rhs);
			this->_proto.set_name(rhs);
			#ifndef NDEBUG
			if (this->_inbuf) {
				this->_inbuf->set_name(rhs);
			}
			if (this->_outbuf) {
				this->_outbuf->set_name(rhs);
			}
			#endif
		}

	};

}

#endif // vim:filetype=cpp
