#ifndef BSCHEDULER_PPL_BASIC_SOCKET_PIPELINE_HH
#define BSCHEDULER_PPL_BASIC_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/net/pstream>

#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/basic_pipeline.hh>

namespace bsc {

	template<class T, class Handler,
	class Kernels=std::queue<T*>,
	class Traits=sys::queue_traits<Kernels>,
	class Threads=std::vector<std::thread>>
	using Proxy_pipeline_base = basic_pipeline<T, Kernels, Traits, Threads,
		//std::mutex, std::unique_lock<std::mutex>,
		sys::spin_mutex, sys::simple_lock<sys::spin_mutex>,
		sys::event_poller<std::shared_ptr<Handler>>>;

	template<class T, class Handler>
	struct basic_socket_pipeline: public Proxy_pipeline_base<T,Handler> {

		typedef Handler event_handler_type;
		typedef typename event_handler_type::clock_type clock_type;
		typedef typename event_handler_type::time_point time_point;
		typedef typename event_handler_type::duration duration;

		typedef Proxy_pipeline_base<T,Handler> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		typedef typename sem_type::handler_type event_handler_ptr;
		typedef typename sem_type::const_iterator handler_const_iterator;

		basic_socket_pipeline(basic_socket_pipeline&& rhs) noexcept:
		base_pipeline(std::move(rhs)),
		_start_timeout(rhs._start_timeout)
		{}

		basic_socket_pipeline():
		base_pipeline(1u)
		{}

		~basic_socket_pipeline() = default;
		basic_socket_pipeline(const basic_socket_pipeline&) = delete;
		basic_socket_pipeline& operator=(const basic_socket_pipeline&) = delete;

	protected:

		inline sem_type&
		poller() noexcept {
			return this->_semaphore;
		}

		inline const sem_type&
		poller() const noexcept {
			return this->_semaphore;
		}

		void
		do_run() override {
			// start processing as early as possible
			poller().notify_one();
			lock_type lock(this->_mutex);
			while (!this->is_stopped()) {
				bool timeout = false;
				if (this->_start_timeout > duration::zero()) {
					handler_const_iterator result =
						this->handler_with_min_start_time_point();
					if (result != this->poller().end()) {
						timeout = true;
						const time_point tp = (*result)->start_time_point()
							+ this->_start_timeout;
						this->poller().wait_until(lock, tp);
					}
				}
				if (!timeout) {
					this->poller().wait(lock);
				}
				sys::unlock_guard<lock_type> g(lock);
				process_kernels_if_any();
				accept_connections_if_any();
				handle_events();
				remove_pipelines_if_any(timeout);
			}
			// prevent double free or corruption
			poller().clear();
		}

		virtual void
		remove_server(sys::fd_type fd) {}

		virtual void
		remove_client(event_handler_ptr ptr) = 0;

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

		inline void
		set_start_timeout(const duration& rhs) noexcept {
			this->_start_timeout = rhs;
		}

	private:

		handler_const_iterator
		handler_with_min_start_time_point() const noexcept {
			handler_const_iterator first =
				std::find_if(
					this->poller().begin(), this->poller().end(),
					[this] (const event_handler_ptr& rhs) {
						return rhs->is_starting();
					}
				);
			return std::min_element(
				first, this->poller().end(),
				[] (const event_handler_ptr& a, const event_handler_ptr& b) {
					return a->start_time_point() < b->start_time_point();
				}
			);
		}

		void
		remove_pipelines_if_any(bool timeout) {
			this->poller().for_each_special_fd(
				[this] (sys::poll_event& ev) {
					if (!ev) {
						this->remove_server(ev.fd());
					}
				}
			);
			const time_point now = timeout
				? clock_type::now()
				: time_point(duration::zero());
			int i=0;
			this->poller().for_each_ordinary_fd(
				[this,timeout,&now,&i] (sys::poll_event& ev, event_handler_ptr& h) {
					if (!ev ||
						h->is_stopped() ||
						(timeout && this->is_timed_out(*h, now))) {
						this->remove_client(h);
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
					try {
						this->accept_connection(ev);
					} catch (const std::exception& err) {
						sys::log_message(
							this->_name,
							"failed to accept connection: _",
							err.what()
						);
					}
				}
			});
		}

		void
		handle_events() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, event_handler_ptr& h) {
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

		bool
		is_timed_out(const event_handler_type& rhs, const time_point& now) {
			return rhs.is_starting() &&
				rhs.start_time_point() + this->_start_timeout <= now;
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
		duration _start_timeout = duration::zero();
		static const int MAX_STOP_ITERATIONS = 13;
	};

}

#endif // vim:filetype=cpp
