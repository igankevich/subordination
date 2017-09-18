#ifndef FACTORY_PPL_REMOTE_CLIENT_HH
#define FACTORY_PPL_REMOTE_CLIENT_HH

#include <memory>
#include <cstddef>

#include <unistdx/base/delete_each>
#include <unistdx/io/fildesbuf>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/pipeline_base.hh>
#include <factory/ppl/kernel_protocol.hh>

namespace factory {

	template<class T, class Socket, class Router>
	class remote_client: public pipeline_base {

	public:
		typedef pipeline_base base_pipeline;
		typedef T kernel_type;
		typedef char Ch;
		typedef basic_kernelbuf<sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>> kernelbuf_type;
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
		_proto()
		{
			this->_proto.set_endpoint(this->_vaddr);
			this->_packetbuf->setfd(std::move(sock));
		}

		remote_client& operator=(const remote_client&) = delete;
		remote_client& operator=(remote_client&&) = delete;
		remote_client(const remote_client&) = delete;
		remote_client(remote_client&& rhs) = delete;

		virtual
		~remote_client() {
			// Here failed kernels are written to buffer,
			// from which they must be recovered with recover_kernels().
			sys::poll_event ev{socket().fd(), sys::poll_event::In};
			this->handle(ev);
			// recover kernels from upstream and downstream buffer
			this->_proto.recover_kernels(this->socket().error());
		}

		void
		send(kernel_type* kernel) {
			this->_stream.rdbuf(this->_packetbuf.get());
			this->_proto.send(kernel, this->_stream);
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			this->_proto.forward(hdr, istr, this->_stream);
		}

		void
		handle(sys::poll_event& event) {
			if (this->is_starting() && !this->socket().error()) {
				this->setstate(pipeline_state::started);
			}
			this->_stream.rdbuf(this->_packetbuf.get());
			this->_stream.sync();
			if (this->_packetbuf->dirty()) {
				event.setev(sys::poll_event::Out);
			} else {
				event.unsetev(sys::poll_event::Out);
			}
			//if (event.in()) {
			this->_proto.receive_kernels(this->_stream);
			//}
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

		const sys::endpoint& vaddr() const { return _vaddr; }
		void setvaddr(const sys::endpoint& rhs) { _vaddr = rhs; }

		friend std::ostream&
		operator<<(std::ostream& out, const remote_client& rhs) {
			return out << sys::make_object(
				"vaddr", rhs.vaddr(),
				"socket", rhs.socket(),
				"state", int(rhs.state())
			);
		}

	};

}

#endif // vim:filetype=cpp
