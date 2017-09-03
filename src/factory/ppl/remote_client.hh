#ifndef FACTORY_PPL_REMOTE_CLIENT_HH
#define FACTORY_PPL_REMOTE_CLIENT_HH

#include <unistdx/base/delete_each>
#include <unistdx/io/fildesbuf>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/pipeline_base.hh>

namespace factory {

	template<class T, class Socket, class Router, class Kernels=std::deque<T*>,
		class Traits=sys::deque_traits<Kernels>>
	struct remote_client: public pipeline_base {

		typedef pipeline_base base_pipeline;
		typedef T kernel_type;
		typedef char Ch;
		typedef basic_kernelbuf<sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>> Kernelbuf;
		typedef kstream<kernel_type> stream_type;
		typedef Socket socket_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef sys::pid_type app_type;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<Kernels,Traits> queue_popper;

		static_assert(
			std::is_move_constructible<stream_type>::value,
			"bad stream_type"
		);

		remote_client() = default;

		remote_client(socket_type&& sock, sys::endpoint vaddr, router_type& router):
		_vaddr(vaddr),
		_packetbuf(),
		_stream(&_packetbuf),
		_sentupstream(),
		_router(router)
		{
			_stream.setforward(
				[this] (app_type app) {
					kernel_header hdr;
					hdr.from(_vaddr);
					hdr.setapp(app);
					_router.forward(hdr, _stream);
				}
			);
			_packetbuf.setfd(std::move(sock));
		}

		remote_client(const remote_client&) = delete;
		remote_client& operator=(const remote_client&) = delete;

		remote_client(remote_client&& rhs):
		base_pipeline(std::move(rhs)),
		_vaddr(rhs._vaddr),
		_packetbuf(std::move(rhs._packetbuf)),
		_stream(std::move(rhs._stream)),
		_sentupstream(std::move(rhs._sentupstream)),
		_sentdownstream(std::move(rhs._sentdownstream)),
		_router(rhs._router)
		{
			this->_stream.rdbuf(&_packetbuf);
		}

		virtual
		~remote_client() {
			this->recover_kernels();
			this->delete_kernels();
		}

		void
		recover_kernels() {

			// Here failed kernels are written to buffer,
			// from which they must be recovered with recover_kernels().
			sys::poll_event ev{socket().fd(), sys::poll_event::In};
			handle(ev);

			// recover kernels from upstream and downstream buffer
			do_recover_kernels(_sentupstream);
			if (socket().error()) {
				do_recover_kernels(_sentdownstream);
			}
		}

		void
		send(kernel_type* kernel) {
			bool delete_kernel = false;
			if (kernel_goes_in_upstream_buffer(kernel)) {
				_sentupstream.push_back(kernel);
			} else
			if (kernel_goes_in_downstream_buffer(kernel)) {
				_sentdownstream.push_back(kernel);
			} else
			if (not kernel->moves_everywhere()) {
				delete_kernel = true;
			}
			sys::log_message("nic", "send to _ kernel _ ", this->vaddr(), *kernel);
			try {
				this->_stream << kernel;
			} catch (const Error& err) {
				sys::log_message("nic", "write error _", err);
			} catch (const std::exception& err) {
				sys::log_message("nic", "write error _", err.what());
			} catch (...) {
				sys::log_message("nic", "write error _", "<unknown>");
			}
			/// The kernel is deleted if it goes downstream
			/// and does not carry its parent.
			if (delete_kernel) {
				delete kernel;
			}
		}

		void
		handle(sys::poll_event& event) {
			this->_stream.clear();
			this->_stream.sync();
			try {
				kernel_type* kernel = nullptr;
				while (this->_stream >> kernel) {
					receive_kernel(kernel);
				}
			} catch (const Error& err) {
				sys::log_message("nic", "read error _", err);
			} catch (const std::exception& err) {
				sys::log_message("nic", "read error _", err.what());
			} catch (...) {
				sys::log_message("nic", "read error _", "<unknown>");
			}
		}

		const socket_type&
		socket() const {
			return _packetbuf.fd();
		}

		socket_type&
		socket() {
			return _packetbuf.fd();
		}

		void
		socket(sys::socket&& rhs) {
			_packetbuf.pubsync();
			_packetbuf.setfd(socket_type(std::move(rhs)));
		}

		const sys::endpoint& vaddr() const { return _vaddr; }
		void setvaddr(const sys::endpoint& rhs) { _vaddr = rhs; }

		friend std::ostream&
		operator<<(std::ostream& out, const remote_client& rhs) {
			return out << sys::make_object(
				"vaddr", rhs.vaddr(),
				"socket", rhs.socket(),
				"kernels", rhs._sentupstream.size()
			);
		}

	private:

		void
		do_recover_kernels(pool_type& rhs) noexcept {
			using namespace std::placeholders;
			std::for_each(
				queue_popper(rhs),
				queue_popper(rhs),
				std::bind(&remote_client::recover_kernel, this, _1)
			);
		}

		void
		delete_kernels() {
			do_delete_kernels(_sentupstream);
			do_delete_kernels(_sentdownstream);
		}

		void
		do_delete_kernels(pool_type& rhs) noexcept {
			sys::delete_each(queue_popper(rhs), queue_popper());
		}

		static bool
		kernel_goes_in_upstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_upstream() or rhs->moves_somewhere();
		}

		static bool
		kernel_goes_in_downstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_downstream() and rhs->carries_parent();
		}

		void
		receive_kernel(kernel_type* k) {
			bool ok = true;
			k->from(_vaddr);
			if (k->moves_downstream()) {
				this->clear_kernel_buffer(k);
			} else if (k->principal_id()) {
				instances_guard g(instances);
				auto result = instances.find(k->principal_id());
				if (result == instances.end()) {
					k->result(exit_code::no_principal_found);
					ok = false;
				}
				k->principal(result->second);
			}
			#ifndef NDEBUG
			sys::log_message("nic", "recv _", *k);
			#endif
			if (!ok) {
				return_kernel(k);
			} else {
				_router.send_local(k);
			}
		}

		void return_kernel(kernel_type* k) {
			#ifndef NDEBUG
			sys::log_message("nic", "no principal found for _", *k);
			#endif
			k->principal(k->parent());
			this->send(k);
		}

		void recover_kernel(kernel_type* k) {
			if (k->moves_upstream()) {
				#ifndef NDEBUG
				sys::log_message("nic", "recover _", *k);
				#endif
				_router.send_remote(k);
			} else if (k->moves_somewhere()) {
				#ifndef NDEBUG
				sys::log_message("nic", "destination is unreachable for _", *k);
				#endif
				k->from(k->to());
				k->result(exit_code::endpoint_not_connected);
				k->principal(k->parent());
				_router.send_local(k);
			} else if (k->moves_downstream() and k->carries_parent()) {
				#ifndef NDEBUG
				sys::log_message("nic", "restore parent _", *k);
				#endif
				_router.send_local(k);
			} else {
				sys::log_message("nic", "bad kernel in sent buffer: _", *k);
			}
		}

		void clear_kernel_buffer(kernel_type* k) {
			auto pos = std::find_if(
				_sentupstream.begin(),
				_sentupstream.end(),
				[k] (kernel_type* rhs) { return *rhs == *k; }
			);
			if (pos != _sentupstream.end()) {
				kernel_type* orig = *pos;
				k->parent(orig->parent());
				k->principal(k->parent());
				delete orig;
				_sentupstream.erase(pos);
			}
		}

		sys::endpoint _vaddr;
		Kernelbuf _packetbuf;
		stream_type _stream;
		pool_type _sentupstream;
		pool_type _sentdownstream;
		router_type& _router;
	};

}

#endif // vim:filetype=cpp
