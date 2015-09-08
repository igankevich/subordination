#ifndef FACTORY_SERVER_NIC_SERVER_HH
#define FACTORY_SERVER_NIC_SERVER_HH

#include <map>

#include <stdx/for_each.hh>
#include <stdx/field_iterator.hh>
#include <stdx/front_popper.hh>

#include <sysx/event.hh>
#include <sysx/socket.hh>
#include <sysx/packstream.hh>

#include <factory/server/intro.hh>
#include <factory/ext/fdbuf.hh>
#include <factory/ext/packetbuf.hh>

namespace factory {

	namespace components {

		template<class T, class Socket, class Kernels=std::deque<typename Server<T>::kernel_type*>>
		struct Remote_Rserver: public Managed_object<Server<T>> {

			typedef char Ch;
			typedef basic_packetbuf<basic_fdbuf<Ch,Socket>> Kernelbuf;
			typedef sysx::basic_packstream<Ch> stream_type;
			typedef Server<T> server_type;
			typedef Socket socket_type;
			typedef Kernels pool_type;
			typedef Managed_object<Server<T>> base_server;
			using typename base_server::kernel_type;
			typedef typename kernel_type::app_type app_type;
			typedef stdx::log<Remote_Rserver> this_log;

			Remote_Rserver(socket_type&& sock, sysx::endpoint vaddr):
				_vaddr(vaddr),
				_packetbuf(),
				_stream(&_packetbuf),
				_buffer(),
				_link(nullptr)
			{
				_packetbuf.setfd(std::move(sock));
			}

			Remote_Rserver(const Remote_Rserver&) = delete;
			Remote_Rserver& operator=(const Remote_Rserver&) = delete;

			Remote_Rserver(Remote_Rserver&& rhs):
				base_server(std::move(rhs)),
				_vaddr(rhs._vaddr),
				_packetbuf(std::move(rhs._packetbuf)),
				_stream(&_packetbuf),
				_buffer(std::move(rhs._buffer)) ,
				_link(rhs._link)
			{
				this_log() << "fd after move ctr " << _packetbuf.fd() << std::endl;
				this_log() << "root after move ctr " << this->root()
					<< ",this=" << this << std::endl;
			}

			virtual
			~Remote_Rserver() {
				stdx::front_pop_iterator<pool_type> it_end;
				std::for_each(stdx::front_popper(_buffer),
					it_end, [] (kernel_type* rhs) { delete rhs; });
			}

			Category
			category() const noexcept override {
				return Category{
					"nic_rserver",
					[] () { return nullptr; }
				};
			}

			void
			recover_kernels() {

				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				on_event(sysx::poll_event::In);

				this_log()
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;
				
				// recover kernels written to output buffer
				using namespace std::placeholders;
				std::for_each(stdx::front_popper(_buffer),
					stdx::front_popper_end(_buffer),
					std::bind(std::mem_fn(&Remote_Rserver::recover_kernel),
					this, _1));
			}

			void send(kernel_type* kernel) {
				if (kernel->result() == Result::NO_PRINCIPAL_FOUND) {
					this_log() << "poll send error: tellp=" << _stream.tellp() << std::endl;
				}
				bool erase_kernel = true;
				if (!kernel->identifiable() && !kernel->moves_everywhere()) {
					kernel->id(this->factory()->factory_generate_id());
					erase_kernel = false;
					this_log() << "Kernel generate id = " << kernel->id() << std::endl;
				}
				if ((kernel->moves_upstream() || kernel->moves_somewhere()) && kernel->identifiable()) {
					_buffer.push_back(kernel);
					erase_kernel = false;
				}
				this->write_kernel(*kernel);
				if (erase_kernel && !kernel->moves_everywhere()) {
					this_log() << "Delete kernel " << *kernel << std::endl;
					delete kernel;
				}
			}

			void
			on_event(sysx::poll_event::event_type e) {
				on_event(sysx::poll_event{socket().fd(), e});
			}

			void
			on_event(sysx::poll_event event) {
				if (event.in()) {
					this_log() << "recv rdstate="
						<< stdx::debug_stream(_stream) << ",event=" << event << std::endl;
					_packetbuf.pubfill();
					while (_packetbuf.update_state()) {
						read_and_send_kernel();
					}
				}
				if (event.out() && !event.hup()) {
					this_log() << "Send rdstate=" << stdx::debug_stream(_stream) << std::endl;
					_stream.flush();
					socket().flush();
					if (!dirty()) {
						this_log() << "Flushed." << std::endl;
					}
				}
				_stream.clear();
			}

			bool
			fail() const {
				return socket().error() != 0;
			}

			bool
			dirty() const {
				return _packetbuf.dirty() || !socket().empty();
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
			socket(sysx::socket&& rhs) {
				_packetbuf.pubfill();
				_stream.clear();
				_packetbuf.setfd(socket_type(std::move(rhs)));
			}

			const sysx::endpoint& vaddr() const { return _vaddr; }
			void setvaddr(const sysx::endpoint& rhs) { _vaddr = rhs; }

			bool empty() const { return _buffer.empty(); }

			Remote_Rserver* link() const { return _link; }
			void link(Remote_Rserver* rhs) { _link = rhs; }

			friend std::ostream&
			operator<<(std::ostream& out, const Remote_Rserver& rhs) {
				return out << "{vaddr="
					<< rhs.vaddr() << ",sock="
					<< rhs.socket() << ",kernels="
					<< rhs._buffer.size() << ",str="
					<< stdx::debug_stream(rhs._stream) << '}';
			}

		private:

			void read_and_send_kernel() {
				app_type app;
				_stream >> app;
				this_log() << "recv ok" << std::endl;
				this_log() << "recv app=" << app << std::endl;
				if (!_stream) return;
				if (app != Application::ROOT) {
					this->app_server()->forward(app, _vaddr, _packetbuf);
				} else {
					Type<kernel_type>::read_object(this->factory()->types(), _stream,
						[this,app] (kernel_type* k) {
							send_kernel(k, app);
						}
					);
				}
			}

			void
			send_kernel(kernel_type* k, app_type app) {
				bool ok = true;
				k->from(_vaddr);
				k->setapp(app);
				this_log()
					<< "recv kernel=" << *k
					<< ",rdstate=" << stdx::debug_stream(_stream)
					<< std::endl;
				if (k->moves_downstream()) {
					this->clear_kernel_buffer(k);
				} else if (k->principal_id()) {
					kernel_type* p = this->factory()->instances().lookup(k->principal_id());
					if (p == nullptr) {
						k->result(Result::NO_PRINCIPAL_FOUND);
						ok = false;
					}
					k->principal(p);
				}
				if (!ok) {
					return_kernel(k);
				} else {
					this->root()->send(k);
				}
			}

			void return_kernel(kernel_type* k) {
				this_log()
					<< "No principal found for "
					<< *k << std::endl;
				k->principal(k->parent());
				// TODO : not safe with bare pointer
//				if (this->link()) {
//					this->link()->send(k);
//				} else {
					this->send(k);
//				}
			}

			void
			write_kernel(kernel_type& kernel) {
				typedef sysx::packstream::pos_type pos_type;
				pos_type old_pos = _stream.tellp();
				_packetbuf.begin_packet();
				_stream << kernel.app();
				Type<kernel_type>::write_object(kernel, _stream);
				_packetbuf.end_packet();
				pos_type new_pos = _stream.tellp();
				this_log() << "send bytes="
					<< new_pos-old_pos
					<< ",stream="
					<< stdx::debug_stream(_stream)
					<< ",krnl=" << kernel
					<< std::endl;
			}

			void recover_kernel(kernel_type* k) {
				if (k->moves_everywhere()) {
					delete k;
				} else {
					k->from(k->to());
					k->result(Result::ENDPOINT_NOT_CONNECTED);
					k->principal(k->parent());
					this->root()->send(k);
				}
			}

			void clear_kernel_buffer(kernel_type* k) {
				auto pos = std::find_if(_buffer.begin(), _buffer.end(),
					[k] (kernel_type* rhs) { return *rhs == *k; });
				if (pos != _buffer.end()) {
					this_log() << "Kernel erased " << k->id() << std::endl;
					kernel_type* orig = *pos;
					k->parent(orig->parent());
					k->principal(k->parent());
					delete orig;
					_buffer.erase(pos);
					this_log() << "Buffer size = " << _buffer.size() << std::endl;
				} else {
					this_log() << "Kernel not found " << k->id() << std::endl;
				}
			}
			
			sysx::endpoint _vaddr;
			Kernelbuf _packetbuf;
			stream_type _stream;
			pool_type _buffer;

			Remote_Rserver* _link;
		};

		template<class T, class Socket,
		class Kernels=std::queue<typename Server<T>::kernel_type*>,
		class Threads=std::vector<std::thread>>
		using NIC_server_base = Server_with_pool<T, Kernels, Threads,
			stdx::spin_mutex, stdx::simple_lock<stdx::spin_mutex>,
			sysx::event_poller<Remote_Rserver<T, Socket>*>>;

		template<class T, class Socket>
		struct NIC_server: public NIC_server_base<T, Socket> {

			typedef Remote_Rserver<T, Socket> server_type;
			typedef std::map<sysx::endpoint, server_type> upstream_type;
			typedef Socket socket_type;

//			typedef bits::handler_wrapper<bits::pointer_tag> wrapper;

			typedef NIC_server_base<T, Socket> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			using typename base_server::kernel_pool;

			typedef server_type* handler_type;
			typedef sysx::event_poller<handler_type> poller_type;
			typedef stdx::log<NIC_server> this_log;

			NIC_server(NIC_server&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			NIC_server():
			base_server(1u)
			{}

			~NIC_server() = default;
			NIC_server(const NIC_server&) = delete;
			NIC_server& operator=(const NIC_server&) = delete;

			void
			do_run() override {
				// start processing as early as possible
				this->process_kernels();
//				while (!this->stopped() && _stop_iterations < MAX_STOP_ITERATIONS) {
				while (!this->stopped()) {
					cleanup_and_check_if_dirty();
					lock_type lock(this->_mutex);
					this->_semaphore.wait(lock,
						[this] () { return this->stopped(); });
					lock.unlock();
					check_and_process_kernels();
					check_and_accept_connections();
					read_and_write_kernels();
				}
				// prevent double free or corruption
				poller().clear();
			}

			void cleanup_and_check_if_dirty() {
				poller().for_each_ordinary_fd(
					[this] (sysx::poll_event& ev, handler_type& h) {
						if (!ev) {
							this->remove_server(h);
						} else {
							if (h->dirty()) {
								ev.setev(sysx::poll_event::Out);
							} else {
								ev.unsetev(sysx::poll_event::Out);
							}
						}
					}
				);
			}

			void
			check_and_process_kernels() {
				if (this->stopped()) {
					this->try_to_stop_gracefully();
				} else {
					poller().for_each_pipe_fd([this] (sysx::poll_event& ev) {
						if (ev.in()) {
							this->process_kernels();
						}
					});
				}
			}

			void
			check_and_accept_connections() {
				poller().for_each_special_fd([this] (sysx::poll_event& ev) {
					if (ev.in()) {
						this->accept_connection();
					}
				});
			}

			void
			read_and_write_kernels() {
				poller().for_each_ordinary_fd(
					[this] (sysx::poll_event& ev, handler_type& h) {
//				TODO: It is probably too slow to check error on every event.
						if (h->fail()) {
							ev.setrev(sysx::poll_event::Hup);
						}
						if (h->dirty()) {
							ev.setrev(sysx::poll_event::Out);
						}
						if (ev) {
							h->on_event(ev);
						}
					}
				);
			}

			void remove_server(server_type* ptr) {
				// remove from the mapping if it is not linked
				// with other subordinate server
				// TODO: occasional ``Bad file descriptor''
				this_log() << "Removing server " << *ptr << std::endl;
				ptr->recover_kernels();
				if (!ptr->link()) {
					_upstream.erase(ptr->vaddr());
				}
			}

			void accept_connection() {
				sysx::endpoint addr;
				socket_type sock;
				_socket.accept(sock, addr);
				sysx::endpoint vaddr = virtual_addr(addr);
				this_log()
					<< "after accept: socket="
					<< sock << std::endl;
				auto res = _upstream.find(vaddr);
				if (res == _upstream.end()) {
					this->add_connected_server(std::move(sock), vaddr, sysx::poll_event::In);
					this_log()
						<< "connected peer "
						<< vaddr << std::endl;
				} else {
					server_type& s = res->second;
					const sysx::port_type
					local_port = s.socket().bind_addr().port();
					this_log()
						<< "ports: "
						<< addr.port() << ' '
						<< local_port
						<< std::endl;
					if (addr.port() < local_port) {
						this_log()
							<< "not replacing peer " << s
							<< std::endl;
						// create temporary subordinate server
						// to read kernels until the socket
						// is closed from the other end
						server_type* new_s = new server_type(std::move(sock), addr);
						this->link_server(&s, new_s,
						sysx::poll_event{new_s->socket().fd(), sysx::poll_event::In});
						debug("not replacing upstream");
					} else {
						this_log log;
						log << "replacing peer " << s;
						poller().disable(s.socket().fd());
						server_type new_s(std::move(s));
						new_s.setparent(this);
						new_s.socket(std::move(sock));
						_upstream.erase(res);
						_upstream.emplace(vaddr, std::move(new_s));
//						_upstream.emplace(vaddr, std::move(*new_s));
						poller().emplace(
							sysx::poll_event{res->second.socket().fd(), sysx::poll_event::Inout, sysx::poll_event::Inout},
							handler_type(&res->second));
						log << " with " << res->second << std::endl;
						debug("replacing upstream");
					}
				}
				this_log()
					<< "after add: socket="
					<< sock << std::endl;
			}

			void try_to_stop_gracefully() {
				this->process_kernels();
				this->flush_kernels();
				++_stop_iterations;
				if (this->empty() || _stop_iterations == MAX_STOP_ITERATIONS) {
					debug("stopping");
				} else {
					debug("not stopping");
				}
			}

			bool empty() const {
				return std::all_of(stdx::field_iter<1>(_upstream.begin()),
					stdx::field_iter<1>(_upstream.end()),
					std::mem_fn(&server_type::empty));
//				typedef typename upstream_type::value_type pair_type;
//				return std::all_of(_upstream.cbegin(), _upstream.cend(),
//					[] (const pair_type& rhs)
//				{
//					return rhs.second.empty();
//				});
			}

			void peer(sysx::endpoint addr) {
//				if (!this->stopped()) {
//					throw Error("Can not add upstream server when socket server is running.",
//						__FILE__, __LINE__, __func__);
//				}
				this->connect_to_server(addr, sysx::poll_event::In);
			}

			void
			socket(const sysx::endpoint& addr) {
				_socket.bind(addr);
				_socket.listen();
				poller().insert_special(sysx::poll_event{_socket.fd(),
					sysx::poll_event::In});
				if (!this->stopped()) {
					this->_semaphore.notify_one();
				}
			}

			inline sysx::endpoint
			server_addr() const {
				return _socket.bind_addr();
			}

			Category
			category() const noexcept override {
				return Category{
					"nic_server",
					[] () { return new NIC_server; },
					{"endpoint"},
					[] (const void* obj, Category::key_type key) {
						const NIC_server* lhs = static_cast<const NIC_server*>(obj);
						if (key == "endpoint") {
							std::stringstream tmp;
							tmp << lhs->server_addr();
							return tmp.str();
						}
						return Category::value_type();
					}
				};
			}
		
		private:

			inline sysx::endpoint
			virtual_addr(sysx::endpoint addr) const {
				return sysx::endpoint(addr, server_addr().port());
			}

			void
			flush_kernels() {
				typedef typename upstream_type::value_type pair_type;
				std::for_each(_upstream.begin(), _upstream.end(),
					[] (pair_type& rhs) {
						rhs.second.on_event(sysx::poll_event::Out);
					}
				);
			}

			void
			process_kernels() {
				this_log() << "NIC_server::process_kernels()" << std::endl;
				stdx::front_pop_iterator<kernel_pool> it_end;
				lock_type lock(this->_mutex);
				stdx::for_each_thread_safe(lock,
					stdx::front_popper(this->_kernels), it_end,
					[this] (kernel_type* rhs) { process_kernel(rhs); }
				);
			}

			void
			process_kernel(kernel_type* k) {
				if (this->server_addr() && k->to() == this->server_addr()) {
					std::ostringstream msg;
					msg << "Kernel is sent to local node. From="
						<< this->server_addr() << ", to=" << k->to();
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}

				if (k->moves_everywhere()) {
					this_log() << "broadcast kernel" << std::endl;
					for (auto& pair : _upstream) {
						pair.second.send(k);
					}
					// delete broadcast kernel
					delete k;
				} else if (k->moves_upstream() && k->to() == sysx::endpoint()) {
					if (_upstream.empty()) {
						throw Error("No upstream servers found.", __FILE__, __LINE__, __func__);
					}
					// TODO: round robin
					auto result = _upstream.begin();
					result->second.send(k);
				} else {
					// create endpoint if necessary, and send kernel
					if (!k->to()) {
						k->to(k->from());
					}
					this->find_or_create_peer(k->to(), sysx::poll_event::Inout)->send(k);
				}
			}

			server_type* find_or_create_peer(const sysx::endpoint& addr, sysx::poll_event::legacy_event ev) {
				server_type* ret;
				auto result = _upstream.find(addr);
				if (result == _upstream.end()) {
					ret = this->connect_to_server(addr, ev);
				} else {
					ret = &result->second;
				}
				return ret;
			}

			struct print_values {
				explicit print_values(const upstream_type& rhs): map(rhs) {}
				friend std::ostream& operator<<(std::ostream& out, const print_values& rhs) {
					out << '{';
					std::copy(stdx::field_iter<1>(rhs.map.begin()),
						stdx::field_iter<1>(rhs.map.end()),
						stdx::intersperse_iterator<server_type>(out, ","));
					out << '}';
					return out;
				}
			private:
				const upstream_type& map;
			};

			void debug(const char* msg = "") {
				this_log()
					<< msg << " upstream " << print_values(_upstream) << std::endl
					<< msg << " events " << poller() << std::endl;
			}

			server_type* connect_to_server(sysx::endpoint addr, sysx::poll_event::legacy_event events) {
				// bind to server address with ephemeral port
				sysx::endpoint srv_addr(this->server_addr(), 0);
				return this->add_connected_server(socket_type(srv_addr, addr), addr, events);
			}

			server_type* add_connected_server(socket_type&& sock, sysx::endpoint vaddr,
				sysx::poll_event::legacy_event events,
				sysx::poll_event::legacy_event revents=0)
			{
				sysx::fd_type fd = sock.fd();
				server_type s(std::move(sock), vaddr);
				s.setparent(this);
				auto result = _upstream.emplace(vaddr, std::move(s));
				poller().emplace(
					sysx::poll_event{fd, events, revents},
					handler_type(&result.first->second));
				this_log() << "added server " << result.first->second << std::endl;
				return &result.first->second;
			}

			void link_server(server_type* par, server_type* sub, sysx::poll_event ev) {
				sub->setparent(this);
				sub->setvaddr(par->vaddr());
				sub->link(par);
				poller().emplace(ev, handler_type(sub));
			}

			inline sem_type&
			poller() {
				return this->_semaphore;
			}

			inline const sem_type&
			poller() const {
				return this->_semaphore;
			}

			socket_type _socket;
			upstream_type _upstream;

			int _stop_iterations = 0;

			static const int MAX_STOP_ITERATIONS = 13;
		};

	}

}

namespace stdx {

	template<class T, class Socket>
	struct type_traits<factory::components::NIC_server<T,Socket>> {
		static constexpr const char*
		short_name() { return "nic_server"; }
		typedef factory::components::server_category category;
	};

	template<class T, class Socket, class Kernels>
	struct type_traits<factory::components::Remote_Rserver<T, Socket, Kernels>> {
		static constexpr const char*
		short_name() { return "nic_rserver"; }
		typedef factory::components::server_category category;
	};

	//template<>
	//struct disable_log_category<server_category>:
	//public std::integral_constant<bool, true> {};

}
#endif // FACTORY_SERVER_NIC_SERVER_HH
