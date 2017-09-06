#ifndef FACTORY_PPL_BASIC_FACTORY_HH
#define FACTORY_PPL_BASIC_FACTORY_HH

#if !defined(FACTORY_DAEMON) && \
	!defined(FACTORY_APPLICATION) && \
	!defined(FACTORY_SUBMIT)
#define FACTORY_APPLICATION
#endif

#include <factory/config.hh>
#include <factory/ppl/parallel_pipeline.hh>
#include <factory/ppl/basic_pipeline.hh>
#include <factory/ppl/io_pipeline.hh>
#include <factory/ppl/multi_pipeline.hh>
#if defined(FACTORY_DAEMON) || defined(FACTORY_SUBMIT)
#include <factory/ppl/socket_pipeline.hh>
#endif
#include <factory/ppl/process_pipeline.hh>
#include <factory/ppl/timer_pipeline.hh>
#include <factory/ppl/application.hh>

namespace factory {

	template <class T>
	struct basic_router {

		static inline void
		send_local(T* rhs);

		static inline void
		send_remote(T*);

		#if defined(FACTORY_DAEMON)
		static inline void
		forward(const kernel_header& hdr, sys::pstream& istr);

		static inline void
		forward_child(const kernel_header& hdr, sys::pstream& istr);

		static inline void
		forward_parent(const kernel_header& hdr, sys::pstream& istr);
		#else
		static inline void
		forward(const kernel_header&, sys::pstream&) {}

		static inline void
		forward_child(const kernel_header&, sys::pstream&) {}

		static inline void
		forward_parent(const kernel_header&, sys::pstream&) {}
		#endif

		#if defined(FACTORY_DAEMON)
		static inline void
		execute(const Application& app);
		#endif

	};

	template <class T>
	class Factory: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef parallel_pipeline<T> cpu_pipeline_type;
		typedef timer_pipeline<T> timer_pipeline_type;
		typedef io_pipeline<T> io_pipeline_type;
		typedef Multi_pipeline<T> downstream_pipeline_type;
		#if defined(FACTORY_APPLICATION)
		typedef child_process_pipeline<T, basic_router<T>>
			parent_pipeline_type;
		#elif defined(FACTORY_DAEMON) || defined(FACTORY_SUBMIT)
		typedef socket_pipeline<T, sys::socket, basic_router<T>>
			parent_pipeline_type;
		#endif
		#if defined(FACTORY_DAEMON)
		typedef unix_domain_socket_pipeline<T, basic_router<T>>
			external_pipeline_type;
		#endif
		#if defined(FACTORY_DAEMON)
		typedef process_pipeline<T, basic_router<T>>
			child_pipeline_type;
		#endif

	private:
		cpu_pipeline_type _upstream;
		downstream_pipeline_type _downstream;
		timer_pipeline_type _timer;
		io_pipeline_type _io;
		#if defined(FACTORY_PRIORITY_SCHEDULING)
		cpu_pipeline_type _prio;
		#endif
		parent_pipeline_type _parent;
		#if defined(FACTORY_DAEMON)
		child_pipeline_type _child;
		external_pipeline_type _external;
		#endif

	public:
		Factory();
		virtual ~Factory() = default;
		Factory(const Factory&) = delete;
		Factory(Factory&&) = delete;

		inline void
		send(kernel_type* kernel) {
			if (kernel->scheduled()) {
				this->_timer.send(kernel);
			} else
			#if defined(FACTORY_PRIORITY_SCHEDULING)
			if (kernel->isset(kernel_flag::priority_service)) {
				this->_prio.send(kernel);
			} else
			#endif
			if (kernel->moves_downstream()) {
				const size_t i = kernel->hash();
				const size_t n = this->_downstream.size();
				this->_downstream[i%n].send(kernel);
			} else {
				this->_upstream.send(kernel);
			}
		}

		inline void
		send_remote(kernel_type* kernel) {
			this->_parent.send(kernel);
		}

		inline void
		send_timer(kernel_type* kernel) {
			this->_timer.send(kernel);
		}

		inline void
		send_timer(kernel_type** kernel, size_t n) {
			this->_timer.send(kernel, n);
		}

		#if defined(FACTORY_DAEMON)
		inline void
		send_child(kernel_type* kernel) {
			this->_child.send(kernel);
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
		forward_child(const kernel_header& hdr, sys::pstream& istr) {
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

		#if defined(FACTORY_DAEMON)
		inline parent_pipeline_type&
		nic() noexcept {
			return this->_parent;
		}

		inline const parent_pipeline_type&
		nic() const noexcept {
			return this->_parent;
		}

		inline void
		forward_parent(const kernel_header& hdr, sys::pstream& istr) {
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

	typedef Factory<FACTORY_KERNEL_TYPE> factory_type;

	extern factory_type factory;

	template <class T>
	void
	basic_router<T>::send_local(T* rhs) {
		factory.send(rhs);
	}

	template <class T>
	void
	basic_router<T>::send_remote(T* rhs) {
		factory.send_remote(rhs);
	}

	#if defined(FACTORY_DAEMON)
	template <class T>
	void
	basic_router<T>::forward(const kernel_header& hdr, sys::pstream& istr) {
		factory.forward_child(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>::forward_child(const kernel_header& hdr, sys::pstream& istr) {
		factory.forward_child(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>::forward_parent(const kernel_header& hdr, sys::pstream& istr) {
		factory.forward_parent(hdr, istr);
	}

	template <class T>
	void
	basic_router<T>::execute(const Application& app) {
		factory.child().add(app);
	}
	#endif

}

#endif // vim:filetype=cpp
