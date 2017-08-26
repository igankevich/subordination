#ifndef FACTORY_PPL_PROXY_SERVER_HH
#define FACTORY_PPL_PROXY_SERVER_HH

#include <memory>

#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/net/pstream>

#include <factory/kernel/kernel_stream.hh>

namespace factory {

	template<class T, class Rserver,
	class Kernels=std::queue<T*>,
	class Traits=sys::queue_traits<Kernels>,
	class Threads=std::vector<std::thread>>
	using Proxy_server_base = Basic_server<T, Kernels, Traits, Threads,
		//std::mutex, std::unique_lock<std::mutex>,
		sys::spin_mutex, sys::simple_lock<sys::spin_mutex>,
		sys::event_poller<std::shared_ptr<Rserver>>>;

	template<class T, class Rserver>
	struct Proxy_server: public Proxy_server_base<T,Rserver> {

		typedef Rserver server_type;

		typedef Proxy_server_base<T,Rserver> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;
		typedef typename sem_type::handler_type server_ptr;

		Proxy_server(Proxy_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		Proxy_server():
		base_server(1u)
		{}

		~Proxy_server() = default;
		Proxy_server(const Proxy_server&) = delete;
		Proxy_server& operator=(const Proxy_server&) = delete;

	protected:

		sem_type&
		poller() {
			return this->_semaphore;
		}

		const sem_type&
		poller() const {
			return this->_semaphore;
		}

		void
		do_run() override {
			// start processing as early as possible
			poller().notify_one();
			lock_type lock(this->_mutex);
			while (!this->is_stopped()) {
				poller().wait(lock);
				sys::unlock_guard<lock_type> g(lock);
				process_kernels_if_any();
				accept_connections_if_any();
				handle_events();
				remove_servers_if_any();
			}
			// prevent double free or corruption
			poller().clear();
		}

		virtual void
		remove_server(server_ptr ptr) = 0;

		virtual void
		process_kernels() = 0;

		virtual void
		accept_connection(sys::poll_event&) {}

		void
		run(Thread_context*) override {
			do_run();
			/**
			do nothing with the context, because we can not
			reliably garbage collect kernels sent from other nodes
			*/
		}

	private:

		void
		remove_servers_if_any() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, server_ptr h) {
					if (!ev) {
						this->remove_server(h);
					}
				}
			);
		}

		void
		process_kernels_if_any() {
			if (this->is_stopped()) {
				this->try_to_stop_gracefully();
			} else {
				poller().for_each_pipe_fd([this] (sys::poll_event& ev) {
					if (ev.in()) {
						this->process_kernels();
					}
				});
			}
		}

		void
		accept_connections_if_any() {
			poller().for_each_special_fd([this] (sys::poll_event& ev) {
				if (ev.in()) {
					this->accept_connection(ev);
				}
			});
		}

		void
		handle_events() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, server_ptr h) {
					h->handle(ev);
				}
			);
		}

		void try_to_stop_gracefully() {
			this->process_kernels();
			// TODO check if this needed at all
			//this->flush_kernels();
			++_stop_iterations;
		}

		//void
		//flush_kernels() {
		//	typedef typename upstream_type::value_type pair_type;
		//	std::for_each(_upstream.begin(), _upstream.end(),
		//		[] (pair_type& rhs) {
		//			rhs.second.handle(sys::poll_event::Out);
		//		}
		//	);
		//}

	protected:

		int _stop_iterations = 0;
		static const int MAX_STOP_ITERATIONS = 13;
	};

}

#endif // vim:filetype=cpp
