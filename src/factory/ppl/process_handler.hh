#ifndef FACTORY_PPL_PROCESS_HANDLER_HH
#define FACTORY_PPL_PROCESS_HANDLER_HH

#include <cassert>
#include <ostream>

#include <unistdx/base/make_object>
#include <unistdx/ipc/process>

#include <factory/kernel/kstream.hh>
#include <factory/ppl/kernel_protocol.hh>
#include <factory/ppl/pipeline_base.hh>

namespace factory {

	template<class T, class Router>
	class process_handler: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef Router router_type;
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<T> stream_type;
		typedef kernel_protocol<T,Router,bits::forward_to_parent<Router>>
			protocol_type;

	private:
		sys::pid_type _childpid;
		kernelbuf_ptr _outbuf;
		stream_type _ostream;
		kernelbuf_ptr _inbuf;
		stream_type _istream;
		protocol_type _proto;

	public:

		process_handler(
			sys::pid_type&& child,
			sys::two_way_pipe&& pipe
		):
		_childpid(child),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto()
		{
			this->_outbuf->setfd(std::move(pipe.parent_out()));
			this->_outbuf->fd().validate();
			this->_inbuf->setfd(std::move(pipe.parent_in()));
			this->_inbuf->fd().validate();
//			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
		}

		process_handler(process_handler&& rhs):
		_childpid(rhs._childpid),
		_outbuf(std::move(rhs._outbuf)),
		_ostream(std::move(rhs._ostream)),
		_inbuf(std::move(rhs._inbuf)),
		_istream(std::move(rhs._istream)),
		_proto(std::move(rhs._proto))
		{
			this->_inbuf->fd().validate();
			this->_outbuf->fd().validate();
			this->_istream.rdbuf(this->_inbuf.get());
			this->_ostream.rdbuf(this->_outbuf.get());
		}

		explicit
		process_handler(sys::pipe&& pipe):
		_childpid(sys::this_process::id()),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto()
		{
			this->_outbuf->setfd(std::move(pipe.out()));
			this->_inbuf->setfd(std::move(pipe.in()));
//			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
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

		void
		close() {
			this->_outbuf->fd().close();
			this->_inbuf->fd().close();
		}

		void
		send(kernel_type* kernel) {
			this->_proto.send(kernel, this->_ostream);
		}

		void
		handle(sys::poll_event& event) {
			if (this->is_starting()) {
				this->setstate(pipeline_state::started);
			}
			if (event.fd() == this->_outbuf->fd()) {
//				this->_ostream.clear();
				this->_ostream.sync();
				if (this->_outbuf->dirty()) {
					event.setev(sys::poll_event::Out);
				} else {
					event.unsetev(sys::poll_event::Out);
				}
			} else {
				assert(
					event.fd() == this->_inbuf->fd()
					|| !this->_inbuf->fd()
					|| !event.bad_fd()
				);
				assert(!event.out() || event.hup());
//				this->_istream.clear();
				this->_istream.sync();
				this->_proto.receive_kernels(this->_istream);
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			this->_proto.forward(hdr, istr, this->_ostream);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const process_handler& rhs) {
			return out << sys::make_object("childpid", rhs._childpid);
		}

	};

}

#endif // vim:filetype=cpp
