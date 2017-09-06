#ifndef FACTORY_PPL_PROCESS_PIPELINE_HH
#define FACTORY_PPL_PROCESS_PIPELINE_HH

#include <map>
#include <cassert>
#include <atomic>
#include <memory>
#include <vector>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/pipe>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <factory/kernel/kernel.hh>
#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/basic_pipeline.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/kernel_protocol.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/application_kernel.hh>

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

	enum Shared_fildes: sys::fd_type {
		In  = 100,
		Out = 101
	};

	template<class T, class Router>
	class process_pipeline:
		public basic_socket_pipeline<T,process_handler<T,Router>> {

	public:
		typedef Router router_type;

		typedef basic_socket_pipeline<T,process_handler<T,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;

		using base_pipeline::poller;

	private:
		typedef std::map<application_type,event_handler_ptr> map_type;
		typedef typename map_type::iterator app_iterator;

	private:
		map_type _apps;
		sys::process_group _procs;

	public:

		process_pipeline() = default;

		process_pipeline(const process_pipeline&) = delete;
		process_pipeline& operator=(const process_pipeline&) = delete;

		process_pipeline(process_pipeline&& rhs) = default;

		~process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override {
			this->_apps.erase(ptr->childpid());
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

		void
		add(const Application& app) {
			lock_type lock(this->_mutex);
			sys::two_way_pipe data_pipe;
			const sys::process& p = _procs.emplace([&app,this,&data_pipe] () {
				data_pipe.close_in_child();
				data_pipe.remap_in_child(Shared_fildes::In, Shared_fildes::Out);
				data_pipe.validate();
				try {
					return app.execute();
				} catch (const std::exception& err) {
					this->log(
						"failed to execute _: _",
						app.filename(),
						err.what()
					);
					// make address sanitizer happy
					#if defined(__SANITIZE_ADDRESS__)
					return sys::this_process::execute_command("false");
					#endif
				}
			});
			#ifndef NDEBUG
			this->log("exec _,pid=_", app, p.id());
			#endif
			data_pipe.close_in_parent();
			data_pipe.validate();
			if (!p) {
				throw std::runtime_error("child process terminated");
			}
			sys::fd_type parent_in = data_pipe.parent_in().get_fd();
			sys::fd_type parent_out = data_pipe.parent_out().get_fd();
			event_handler_ptr child =
				std::make_shared<event_handler_type>(
					p.id(),
					std::move(data_pipe)
				);
			child->set_name(this->_name);
			// child.setparent(this);
			// assert(child.root() != nullptr);
			auto result = this->_apps.emplace(app.id(), child);
			poller().emplace(
				sys::poll_event{parent_in, sys::poll_event::In, 0},
				result.first->second
			);
			poller().emplace(sys::poll_event{parent_out, 0, 0}, result.first->second);
		}

		void
		do_run() override {
			std::thread waiting_thread{
				&process_pipeline::wait_for_all_processes_to_finish,
				this
			};
			base_pipeline::do_run();
			#ifndef NDEBUG
			this->log(
				"waiting for all processes to finish: pid=_",
				sys::this_process::id()
			);
			#endif
			if (waiting_thread.joinable()) {
				waiting_thread.join();
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			this->log("fwd _", hdr);
			app_iterator result = this->find_by_app_id(hdr.app());
			if (result == this->_apps.end()) {
				FACTORY_THROW(Error, "bad application id");
			}
			result->second->forward(hdr, istr);
		}

	private:

		void
		process_kernel(kernel_type* k) {
			typedef typename map_type::value_type value_type;
			if (k->moves_everywhere()) {
				std::for_each(this->_apps.begin(), this->_apps.end(),
					[k] (value_type& rhs) {
						rhs.second->send(k);
					}
				);
			} else {
				app_iterator result = this->find_by_app_id(k->app());
				if (result == this->_apps.end()) {
					FACTORY_THROW(Error, "bad application id");
				}
				result->second->send(k);
			}
		}

		void
		wait_for_all_processes_to_finish() {
			using std::this_thread::sleep_for;
			using std::chrono::milliseconds;
			lock_type lock(this->_mutex);
			while (!this->is_stopped()) {
				sys::unlock_guard<lock_type> g(lock);
				using namespace std::placeholders;
				if (this->_procs.size() == 0) {
					sleep_for(milliseconds(777));
				} else {
					this->_procs.wait(
						std::bind(&process_pipeline::on_process_exit, this, _1, _2)
					);
				}
			}
		}

		void
		on_process_exit(const sys::process& p, sys::proc_info status) {
			this->log("process exited: _", status);
			lock_type lock(this->_mutex);
			app_iterator result = this->find_by_process_id(p.id());
			if (result != this->_apps.end()) {
				#ifndef NDEBUG
				this->log("app finished: app=_", result->first);
				#endif
				result->second->close();
			}
		}

		app_iterator
		find_by_app_id(application_type id) {
			return this->_apps.find(id);
		}

		app_iterator
		find_by_process_id(sys::pid_type pid) {
			typedef typename map_type::value_type value_type;
			return std::find_if(
				this->_apps.begin(),
				this->_apps.end(),
				[pid] (const value_type& rhs) {
					return rhs.second->childpid() == pid;
				}
			);
		}

	};

	template<class T, class Router>
	struct child_process_pipeline:
		public basic_socket_pipeline<T,process_handler<T,Router>> {

		typedef basic_socket_pipeline<T,process_handler<T,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		typedef Router router_type;

		using base_pipeline::poller;

		child_process_pipeline():
		_parent(std::make_shared<event_handler_type>(
			sys::pipe{Shared_fildes::In, Shared_fildes::Out}
		))
		{}

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual ~child_process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override {
			if (!this->is_stopped()) {
				this->stop();
				// this->factory()->stop();
			}
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

		void
		do_run() override {
			this->init_pipeline();
			base_pipeline::do_run();
		}

	private:

		void
		init_pipeline() {
			// _parent.setparent(this);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::In, sys::poll_event::In, 0},
				this->_parent
			);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::Out, 0, 0},
				this->_parent
			);
			this->_parent->set_name(this->name());
		}

		void
		process_kernel(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
			this->_parent->send(k);
		}

		event_handler_ptr _parent;
	};

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
			std::clog << "event=" << event << std::endl;
			std::clog << "socket()=" << socket() << std::endl;
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

	template<class T, class Router>
	class unix_domain_socket_pipeline:
		public basic_socket_pipeline<T,external_process_handler<T,Router>> {

	public:
		typedef basic_socket_pipeline<T,external_process_handler<T,Router>>
			base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		typedef Router router_type;

		using base_pipeline::poller;

	private:
		typedef std::pair<sys::endpoint, sys::socket> server_type;
		typedef std::vector<server_type> server_container;
		typedef server_container::iterator server_iterator;
		typedef event_handler_ptr client_type;
		typedef typename sem_type::const_iterator client_iterator;

	private:
		server_container _servers;

	public:
		unix_domain_socket_pipeline() = default;
		~unix_domain_socket_pipeline() = default;
		unix_domain_socket_pipeline(const unix_domain_socket_pipeline& rhs) = delete;
		unix_domain_socket_pipeline(unix_domain_socket_pipeline&& rhs) = delete;

		void
		add_server(const sys::endpoint& rhs) {
			lock_type lock(this->_mutex);
			if (this->find_server(rhs) == this->_servers.end()) {
				sys::socket sock;
				sock.bind(rhs);
				sock.setopt(sys::socket::pass_credentials);
				sock.listen();
				this->_servers.emplace_back(rhs, std::move(sock));
				server_type& srv = this->_servers.back();
				sys::fd_type fd = srv.second.get_fd();
				this->poller().insert_special(
					sys::poll_event{fd, sys::poll_event::In}
				);
				if (!this->is_stopped()) {
					this->_semaphore.notify_one();
				}
			}
		}

		void
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			sys::socket s(sys::family_type::unix);
			s.setopt(sys::socket::pass_credentials);
			s.connect(addr);
			this->add_client(addr, std::move(s));
			this->poller().notify_one();
		}

	protected:

		void
		remove_server(sys::fd_type fd) override {
			server_iterator result = this->find_server(fd);
			if (result != this->_servers.end()) {
				#ifndef NDEBUG
				this->log("remove server _", result->first);
				#endif
				this->_servers.erase(result);
			}
		}

		void
		remove_client(event_handler_ptr ptr) override {
			#ifndef NDEBUG
			this->log("remove _ _", ptr->endpoint(), ptr->socket());
			#endif
		}

		void
		accept_connection(sys::poll_event& ev) override {
			sys::endpoint addr;
			sys::socket sock;
			server_iterator result = this->find_server(ev.fd());
			if (result == this->_servers.end()) {
				throw std::invalid_argument("no matching server found");
			}
			result->second.accept(sock, addr);
			client_iterator res = this->find_client(addr);
			if (res != this->poller().end()) {
				throw std::invalid_argument("client already exists");
			}
			this->add_client(addr, std::move(sock));
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

	private:

		void
		process_kernel(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
		}

		server_iterator
		find_server(const sys::endpoint& e) {
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&e] (const server_type& rhs) {
					return rhs.first == e;
				}
			);
		}

		server_iterator
		find_server(sys::fd_type fd) {
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[fd] (const server_type& rhs) {
					return rhs.second.get_fd() == fd;
				}
			);
		}

		client_iterator
		find_client(const sys::endpoint& e) {
			return std::find_if(
				this->poller().begin(),
				this->poller().end(),
				[&e] (const client_type& rhs) {
					return rhs->endpoint() == e;
				}
			);
		}

		void
		add_client(const sys::endpoint& addr, sys::socket&& sock) {
			sys::fd_type fd = sock.fd();
			event_handler_ptr s =
				std::make_shared<event_handler_type>(
					addr,
					std::move(sock)
				);
			s->setstate(pipeline_state::starting);
			this->poller().emplace(
				sys::poll_event{fd, sys::poll_event::In, sys::poll_event::In},
				s
			);
			#ifndef NDEBUG
			this->log("add _", addr);
			#endif
		}

	};

}

#endif // vim:filetype=cpp
