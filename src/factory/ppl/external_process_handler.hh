#ifndef FACTORY_PPL_EXTERNAL_PROCESS_HANDLER_HH
#define FACTORY_PPL_EXTERNAL_PROCESS_HANDLER_HH

#include <memory>

#include <unistdx/io/fildesbuf>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <factory/kernel/kstream.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/application_kernel.hh>
#include <factory/ppl/pipeline_base.hh>

namespace factory {

	template<class T, class Router>
	class external_process_handler: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef Router router_type;

	private:
		typedef basic_kernelbuf<
			sys::basic_fildesbuf<char, std::char_traits<char>, sys::socket>>
			kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<T> stream_type;
		typedef sys::ipacket_guard<stream_type> ipacket_guard;
		typedef sys::opacket_guard<stream_type> opacket_guard;

	private:
		sys::endpoint _endpoint;
		kernelbuf_ptr _buffer;
		stream_type _stream;

	public:

		inline
		external_process_handler(const sys::endpoint& e, sys::socket&& sock):
		_endpoint(e),
		_buffer(new kernelbuf_type),
		_stream(_buffer.get())
		{
			this->_buffer->setfd(std::move(sock));
		}

		const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

		inline sys::socket&
		socket() noexcept {
			return this->_buffer->fd();
		}

		void
		handle(sys::poll_event& event) {
			if (this->is_starting()) {
				this->setstate(pipeline_state::started);
			}
			this->_stream.sync();
			this->receive_kernels(this->_stream);
			this->_stream.sync();
		}

	private:

		void
		receive_kernels(stream_type& stream) noexcept {
			while (stream.read_packet()) {
				Application_kernel* k = nullptr;
				try {
					// eats remaining bytes on exception
					application_type a;
					ipacket_guard g(stream);
					kernel_type* kernel = nullptr;
					stream >> a;
					stream >> kernel;
					k = dynamic_cast<Application_kernel*>(kernel);
					this->log("recv _", *k);
					Application app(k->arguments(), k->environment());
					sys::user_credentials creds = this->socket().credentials();
					app.set_credentials(creds.uid, creds.gid);
					try {
						router_type::execute(app);
						k->return_to_parent(exit_code::success);
					} catch (const std::exception& err) {
						k->return_to_parent(exit_code::error);
						k->set_error(err.what());
					} catch (...) {
						k->return_to_parent(exit_code::error);
						k->set_error("unknown error");
					}
				} catch (const Error& err) {
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
						stream << this_application::get_id();
						stream << k;
						stream.end_packet();
					} catch (const Error& err) {
						sys::log_message("proto", "write error _", err);
					} catch (const std::exception& err) {
						sys::log_message("proto", "write error _", err.what());
					} catch (...) {
						sys::log_message("proto", "write error _", "<unknown>");
					}
					delete k;
				}
			}
		}

	};

}

#endif // vim:filetype=cpp
