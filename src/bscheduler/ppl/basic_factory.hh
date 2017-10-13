#ifndef BSCHEDULER_PPL_BASIC_FACTORY_HH
#define BSCHEDULER_PPL_BASIC_FACTORY_HH

#if !defined(BSCHEDULER_DAEMON) && \
    !defined(BSCHEDULER_APPLICATION) && \
    !defined(BSCHEDULER_SUBMIT)
#define BSCHEDULER_APPLICATION
#endif

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_pipeline.hh>
#include <bscheduler/ppl/io_pipeline.hh>
#include <bscheduler/ppl/multi_pipeline.hh>
#include <bscheduler/ppl/parallel_pipeline.hh>
#if defined(BSCHEDULER_DAEMON) || defined(BSCHEDULER_SUBMIT)
#include <bscheduler/ppl/socket_pipeline.hh>
#include <unistdx/net/socket>
#endif
#include <bscheduler/ppl/process_pipeline.hh>
#if defined(BSCHEDULER_APPLICATION)
#include <bscheduler/ppl/child_process_pipeline.hh>
#endif
#if defined(BSCHEDULER_DAEMON)
#include <bscheduler/ppl/unix_domain_socket_pipeline.hh>
#endif
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_router.hh>
#include <bscheduler/ppl/timer_pipeline.hh>

namespace bsc {

	template <class T>
	class Factory: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef parallel_pipeline<T> cpu_pipeline_type;
		typedef timer_pipeline<T> timer_pipeline_type;
		typedef io_pipeline<T> io_pipeline_type;
		typedef Multi_pipeline<T> downstream_pipeline_type;
		#if defined(BSCHEDULER_APPLICATION)
		typedef child_process_pipeline<T, basic_router<T>>
		    parent_pipeline_type;
		#elif defined(BSCHEDULER_DAEMON) || defined(BSCHEDULER_SUBMIT)
		typedef socket_pipeline<T, sys::socket, basic_router<T>>
		    parent_pipeline_type;
		#endif
		#if defined(BSCHEDULER_DAEMON) && \
		!defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
		typedef unix_domain_socket_pipeline<T, basic_router<T>>
		    external_pipeline_type;
		#endif
		#if defined(BSCHEDULER_DAEMON) && \
		!defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
		typedef process_pipeline<T, basic_router<T>>
		    child_pipeline_type;
		#endif

	private:
		cpu_pipeline_type _upstream;
		downstream_pipeline_type _downstream;
		timer_pipeline_type _timer;
		#if !defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
		io_pipeline_type _io;
		#endif
		parent_pipeline_type _parent;
		#if defined(BSCHEDULER_DAEMON) && \
		!defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
		child_pipeline_type _child;
		external_pipeline_type _external;
		#endif

	public:
		Factory();

		virtual
		~Factory() = default;

		Factory(const Factory&) = delete;

		Factory(Factory&&) = delete;

		inline void
		send(kernel_type* k) {
			if (k->scheduled()) {
				this->_timer.send(k);
			} else if (k->moves_downstream()) {
				const size_t i = k->hash();
				const size_t n = this->_downstream.size();
				this->_downstream[i%n].send(k);
			} else {
				this->_upstream.send(k);
			}
		}

		inline void
		send_remote(kernel_type* k) {
			this->_parent.send(k);
		}

		inline void
		send_timer(kernel_type* k) {
			this->_timer.send(k);
		}

		inline void
		send_timer(kernel_type** k, size_t n) {
			this->_timer.send(k, n);
		}

		#if defined(BSCHEDULER_DAEMON) && \
		!defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
		inline void
		send_child(kernel_type* k) {
			this->_child.send(k);
		}

		inline child_pipeline_type&
		child() noexcept {
			return this->_child;
		}

		inline const child_pipeline_type&
		child() const noexcept {
			return this->_child;
		}

		inline void
		forward_child(kernel_header& hdr, sys::pstream& istr) {
			this->_child.forward(hdr, istr);
		}

		inline external_pipeline_type&
		external() noexcept {
			return this->_external;
		}

		inline const external_pipeline_type&
		external() const noexcept {
			return this->_external;
		}

		#endif

		inline parent_pipeline_type&
		parent() noexcept {
			return this->_parent;
		}

		inline const parent_pipeline_type&
		parent() const noexcept {
			return this->_parent;
		}

		#if defined(BSCHEDULER_DAEMON)
		inline parent_pipeline_type&
		nic() noexcept {
			return this->_parent;
		}

		inline const parent_pipeline_type&
		nic() const noexcept {
			return this->_parent;
		}

		inline void
		forward_parent(kernel_header& hdr, sys::pstream& istr) {
			this->_parent.forward(hdr, istr);
		}

		#endif

		inline timer_pipeline_type&
		timer() noexcept {
			return this->_timer;
		}

		inline const timer_pipeline_type&
		timer() const noexcept {
			return this->_timer;
		}

		void
		start();

		void
		stop();

		void
		wait();

	};

	typedef Factory<BSCHEDULER_KERNEL_TYPE> factory_type;

	extern factory_type factory;

	template <class T>
	void
	basic_router<T>
	::send_local(T* rhs) {
		factory.send(rhs);
	}

	template <class T>
	void
	basic_router<T>
	::send_remote(T* rhs) {
		factory.send_remote(rhs);
	}

	#if defined(BSCHEDULER_DAEMON) && \
	!defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
	template <class T>
	void
	basic_router<T>
	::forward(kernel_header& hdr, sys::pstream& istr) {
		factory.forward_child(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>
	::forward_child(kernel_header& hdr, sys::pstream& istr) {
		factory.forward_child(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>
	::forward_parent(kernel_header& hdr, sys::pstream& istr) {
		factory.forward_parent(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>
	::execute(const application& app) {
		factory.child().add(app);
	}

	#else
	template <class T>
	void
	basic_router<T>
	::forward(kernel_header& hdr, sys::pstream& istr) {}

	template <class T>
	void
	basic_router<T>
	::forward_child(kernel_header& hdr, sys::pstream& istr) {}

	template <class T>
	void
	basic_router<T>
	::forward_parent(kernel_header& hdr, sys::pstream& istr) {}

	template <class T>
	void
	basic_router<T>
	::execute(const application& app) {}
	#endif // if defined(BSCHEDULER_DAEMON)

}

#endif // vim:filetype=cpp
