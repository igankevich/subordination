#ifndef BSCHEDULER_PPL_REMOTE_CLIENT_HH
#define BSCHEDULER_PPL_REMOTE_CLIENT_HH

#include <cstddef>
#include <memory>

#include <unistdx/base/delete_each>
#include <unistdx/io/fildesbuf>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <bscheduler/kernel/kernel_instance_registry.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/kernel_protocol.hh>
#include <bscheduler/ppl/pipeline_base.hh>

namespace bsc {

	template<class T, class Socket, class Router>
	class remote_client: public pipeline_base {

	public:
		typedef pipeline_base base_pipeline;
		typedef T kernel_type;
		typedef char Ch;
		typedef sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>
			fildesbuf_type;
		typedef basic_kernelbuf<fildesbuf_type> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<T> stream_type;
		typedef kernel_protocol<T,Router,bits::forward_to_child<Router>>
		    protocol_type;
		typedef Socket socket_type;
		typedef Router router_type;
		typedef sys::pid_type app_type;
		typedef uint32_t weight_type;

		static_assert(
			std::is_move_constructible<stream_type>::value,
			"bad stream_type"
		);

	private:
		sys::endpoint _vaddr;
		kernelbuf_ptr _packetbuf;
		stream_type _stream;
		protocol_type _proto;
		/// The number of nodes "behind" this one in the hierarchy.
		weight_type _weight = 1;

	public:
		remote_client() = default;

		remote_client(socket_type&& sock, sys::endpoint vaddr):
		_vaddr(vaddr),
		_packetbuf(new kernelbuf_type),
		_stream(_packetbuf.get()),
		_proto() {
			this->_proto.setf(kernel_proto_flag::prepend_application);
			this->_proto.set_endpoint(this->_vaddr);
			this->_packetbuf->setfd(std::move(sock));
		}

		remote_client&
		operator=(const remote_client&) = delete;

		remote_client&
		operator=(remote_client&&) = delete;

		remote_client(const remote_client&) = delete;

		remote_client(remote_client&& rhs) = delete;

		virtual
		~remote_client() {
			// Here failed kernels are written to buffer,
			// from which they must be recovered with recover_kernels().
			sys::epoll_event ev {socket().fd(), sys::event::in};
			this->handle(ev);
			// recover kernels from upstream and downstream buffer
			this->_proto.recover_kernels(this->socket().error());
		}

		void
		send(kernel_type* k) {
			this->_stream.rdbuf(this->_packetbuf.get());
			this->_proto.send(k, this->_stream);
		}

		void
		forward(kernel_header& hdr, sys::pstream& istr) {
			this->_proto.forward(hdr, istr, this->_stream);
		}

		void
		handle(sys::epoll_event& event) {
			if (this->is_starting() && !this->socket().error()) {
				this->setstate(pipeline_state::started);
				event.setev(sys::event::out);
			}
			this->_stream.rdbuf(this->_packetbuf.get());
			this->_stream.sync();
			this->_proto.receive_kernels(this->_stream);
//			if (this->_packetbuf->dirty()) {
//				event.setev(sys::event::out);
//			} else {
//				event.unsetev(sys::event::out);
//			}
		}

		inline const socket_type&
		socket() const noexcept {
			return this->_packetbuf->fd();
		}

		inline socket_type&
		socket() noexcept {
			return this->_packetbuf->fd();
		}

		void
		socket(sys::socket&& rhs) {
			this->_packetbuf->pubsync();
			this->_packetbuf->setfd(socket_type(std::move(rhs)));
		}

		inline weight_type
		weight() const noexcept {
			return this->_weight;
		}

		inline void
		weight(weight_type rhs) noexcept {
			this->_weight = rhs;
		}

		inline const sys::endpoint&
		vaddr() const noexcept {
			return this->_vaddr;
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

		friend std::ostream&
		operator<<(std::ostream& out, const remote_client& rhs) {
			return out << sys::make_object(
				"vaddr",
				rhs.vaddr(),
				"socket",
				rhs.socket(),
				"state",
				int(rhs.state()),
				"weight",
				rhs.weight()
			    );
		}

	};

}

#endif // vim:filetype=cpp
