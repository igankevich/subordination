#ifndef BSCHEDULER_PPL_BASIC_SOCKET_PIPELINE_HH
#define BSCHEDULER_PPL_BASIC_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
//#include <unistdx/base/recursive_spin_mutex>
//#include <unistdx/base/simple_lock>
//#include <unistdx/base/spin_mutex>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller2>
#include <unistdx/net/pstream>

#include <bscheduler/base/static_lock.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/basic_handler.hh>
#include <bscheduler/ppl/basic_pipeline.hh>

namespace bsc {

	template<class T,
	class Kernels=std::queue<T*>,
	class Traits=sys::queue_traits<Kernels>,
	class Threads=std::vector<std::thread>>
	using Proxy_pipeline_base = basic_pipeline<T, Kernels, Traits, Threads,
		std::recursive_mutex, std::unique_lock<std::recursive_mutex>,
//		sys::recursive_spin_mutex, sys::simple_lock<sys::recursive_spin_mutex>,
		sys::event_poller2>;

	template<class T>
	class basic_socket_pipeline: public Proxy_pipeline_base<T> {

	public:
		typedef basic_handler handler_type;
		typedef std::shared_ptr<handler_type> event_handler_ptr;
		typedef std::unordered_map<sys::fd_type,event_handler_ptr>
			handler_container_type;
		typedef typename handler_container_type::const_iterator
			handler_const_iterator;
		typedef typename handler_type::clock_type clock_type;
		typedef typename handler_type::time_point time_point;
		typedef typename handler_type::duration duration;

		typedef Proxy_pipeline_base<T> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;

	private:
		typedef static_lock<mutex_type, mutex_type> static_lock_type;

	private:
		mutex_type* _othermutex = nullptr;

	protected:
		handler_container_type _handlers;

	public:

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

		inline void
		set_other_mutex(mutex_type* rhs) noexcept {
			this->_othermutex = rhs;
		}

		inline mutex_type*
		other_mutex() noexcept {
			return this->_othermutex;
		}

		inline mutex_type*
		mutex() noexcept {
			return &this->_mutex;
		}

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
		emplace_handler(
			const sys::epoll_event& ev,
			const event_handler_ptr& ptr
		) {
			this->log("add _", *ptr);
			lock_type lock(this->_mutex);
			this->_handlers.emplace(ev.fd(), ptr);
			this->_poller.emplace(ev.fd());
		}

		template <class X>
		void
		emplace_handler(
			const sys::epoll_event& ev,
			const std::shared_ptr<X>& ptr
		) {
			this->emplace_handler(ev, std::static_pointer_cast<handler_type>(ptr));
		}

		void
		emplace_notify_handler(const event_handler_ptr& ptr) {
			lock_type lock(this->_mutex);
			this->_handlers.emplace(this->poller().pipe_in(), ptr);
		}

		template <class X>
		void
		emplace_notify_handler(const std::shared_ptr<X>& ptr) {
			this->_handlers.emplace(
				this->poller().pipe_in(),
				std::static_pointer_cast<handler_type>(ptr)
			);
		}

		void
		do_run() override {
			static_lock_type lock(&this->_mutex, this->_othermutex);
			while (!this->has_stopped()) {
				bool timeout = false;
				if (this->_start_timeout > duration::zero()) {
					handler_const_iterator result =
						this->handler_with_min_start_time_point();
					if (result != this->_handlers.end()) {
						timeout = true;
						const time_point tp = result->second->start_time_point()
							+ this->_start_timeout;
						this->poller().wait_until(lock, tp);
					}
				}
				if (!timeout) {
					this->poller().wait(lock);
				}
				this->handle_events(timeout);
			}
		}

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
			typedef typename handler_container_type::value_type value_type;
			handler_const_iterator end = this->_handlers.end();
			handler_const_iterator first =
				std::find_if(
					this->_handlers.begin(), end,
					[this] (const value_type& rhs) {
						return rhs.second->is_starting();
					}
				);
			return std::min_element(
				first, end,
				[] (const value_type& a, const value_type& b) {
					return a.second->start_time_point()
						 < b.second->start_time_point();
				}
			);
		}

		void
		handle_events(bool timeout) {
			const time_point now = timeout
				? clock_type::now()
				: time_point(duration::zero());
			for (const sys::epoll_event& ev : this->poller()) {
				auto result = this->_handlers.find(ev.fd());
				if (result == this->_handlers.end()) {
					this->log("unable to process fd _", ev.fd());
				} else {
					handler_type& h = *result->second;
					// process event by calling event handler function
					try {
						h.handle(ev);
					} catch (const std::exception& err) {
						this->log("failed to process fd _: _", ev.fd(), err.what());
					}
					if (!ev ||
						h.has_stopped() ||
						(timeout && this->is_timed_out(h, now))) {
						this->log("remove _", h);
						this->_handlers.erase(result);
					}
				}
			}
		}

		bool
		is_timed_out(const handler_type& rhs, const time_point& now) {
			return rhs.is_starting() &&
				rhs.start_time_point() + this->_start_timeout <= now;
		}

	protected:

		duration _start_timeout = duration::zero();
		static const int MAX_STOP_ITERATIONS = 13;
	};

}

#endif // vim:filetype=cpp
