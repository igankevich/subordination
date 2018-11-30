#include "unix_domain_socket_pipeline.hh"

#include <unistdx/io/fildesbuf>

#include <bscheduler/config.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/application_kernel.hh>
#include <bscheduler/ppl/basic_router.hh>

namespace bsc {

	template <class K, class R>
	class unix_socket_server: public basic_handler {

	private:
		typedef unix_domain_socket_pipeline<K,R> this_type;

	private:
		sys::socket _socket;
		sys::socket_address _endpoint;
		this_type& _ppl;

	public:

		unix_socket_server(const sys::socket_address& endp, this_type& ppl):
		_endpoint(endp),
		_ppl(ppl)
		{
			this->_socket.bind(endp);
			this->_socket.setopt(sys::socket::pass_credentials);
			this->_socket.listen();
			this->log("socket=_", this->_socket);
		}

		void
		handle(const sys::epoll_event& ev) override {
			this->log("_ _", __func__, ev);
			sys::socket_address addr;
			sys::socket sock;
			this->_socket.accept(sock, addr);
			this->_ppl.add_client(addr, std::move(sock));
		};

		void
		write(std::ostream& out) const override {
			out << "server " << this->_endpoint;
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.fd();
		}

	};

	template <class K, class R>
	class unix_socket_client: public basic_handler {

	public:
		typedef K kernel_type;
		typedef R router_type;

	private:
		typedef basic_kernelbuf<
			sys::basic_fildesbuf<char, std::char_traits<char>, sys::socket>>
			kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<kernel_type> stream_type;
		typedef typename stream_type::ipacket_guard ipacket_guard;
		typedef sys::opacket_guard<stream_type> opacket_guard;

	private:
		sys::socket_address _endpoint;
		kernelbuf_ptr _buffer;
		stream_type _stream;

	public:

		explicit
		unix_socket_client(const sys::socket_address& endp):
		_endpoint(endp),
		_buffer(new kernelbuf_type),
		_stream(_buffer.get())
		{
			sys::socket s(sys::family_type::unix);
			s.setopt(sys::socket::pass_credentials);
			s.connect(endp);
			this->_buffer->setfd(std::move(s));
			this->setstate(pipeline_state::starting);
		}

		unix_socket_client(const sys::socket_address& endp, sys::socket&& sock):
		_endpoint(endp),
		_buffer(new kernelbuf_type),
		_stream(_buffer.get())
		{
			this->_buffer->setfd(std::move(sock));
			this->setstate(pipeline_state::starting);
		}

		void
		handle(const sys::epoll_event& ev) override {
			this->log("_ _", __func__, ev);
			if (this->is_starting()) {
				this->setstate(pipeline_state::started);
			}
			this->_stream.sync();
			this->receive_kernels(this->_stream);
			this->_stream.sync();
		};

		void
		write(std::ostream& out) const override {
			out << "client " << this->_endpoint << ' ' << this->_buffer->fd();
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_buffer->fd().fd();
		}

		inline const sys::socket&
		socket() const noexcept {
			return this->_buffer->fd();
		}

	private:

		void
		receive_kernels(stream_type& stream) {
			while (stream.read_packet()) {
				Application_kernel* k = nullptr;
				try {
					// eats remaining bytes on exception
					kernel_header::application_ptr ptr = nullptr;
					kernel_header hdr;
					ipacket_guard g(stream.rdbuf());
					kernel_type* tmp = nullptr;
					stream >> hdr;
					stream >> tmp;
					k = dynamic_cast<Application_kernel*>(tmp);
					#ifndef NDEBUG
					this->log("recv _", *k);
					#endif
					application app(k->arguments(), k->environment());
					sys::user_credentials creds = this->socket().credentials();
					app.workdir(k->workdir());
					app.set_credentials(creds.uid, creds.gid);
					app.make_master();
					try {
						k->application(app.id());
						router_type::execute(app);
						k->return_to_parent(exit_code::success);
					} catch (const sys::bad_call& err) {
						k->return_to_parent(exit_code::error);
						k->set_error(err.what());
						this->log("execute error _,app=_", err, app.id());
					} catch (const std::exception& err) {
						k->return_to_parent(exit_code::error);
						k->set_error(err.what());
						this->log("execute error _,app=_", err.what(), app.id());
					} catch (...) {
						k->return_to_parent(exit_code::error);
						k->set_error("unknown error");
						this->log("execute error _", "<unknown>");
					}
				} catch (const error& err) {
					this->log("read error _", err);
				} catch (const std::exception& err) {
					this->log("read error _ ", err.what());
				} catch (...) {
					this->log("read error _", "<unknown>");
				}
				if (k) {
					try {
						opacket_guard g(stream);
						stream.begin_packet();
						kernel_header hdr;
						hdr.setapp(this_application::get_id());
						stream << hdr;
						stream << k;
						stream.end_packet();
					} catch (const error& err) {
						this->log("write error _", err);
					} catch (const std::exception& err) {
						this->log("write error _", err.what());
					} catch (...) {
						this->log("write error _", "<unknown>");
					}
					delete k;
				}
			}
		}

	};

}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_client(const sys::socket_address& addr, sys::socket&& sock) {
	auto ptr =
		std::make_shared<unix_socket_client<K,R>>(addr, std::move(sock));
	this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
	#ifndef NDEBUG
	this->log("add _", addr);
	#endif
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_client(const sys::socket_address& addr) {
	auto ptr = std::make_shared<unix_socket_client<K,R>>(addr);
	this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_server(const sys::socket_address& rhs) {
	auto ptr = std::make_shared<unix_socket_server<K,R>>(rhs, *this);
	this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
}

template class bsc::unix_domain_socket_pipeline<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
template class bsc::unix_socket_server<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
template class bsc::unix_socket_client<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
