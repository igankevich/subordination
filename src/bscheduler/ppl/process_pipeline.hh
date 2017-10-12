#ifndef BSCHEDULER_PPL_PROCESS_PIPELINE_HH
#define BSCHEDULER_PPL_PROCESS_PIPELINE_HH

#include <map>
#include <memory>

#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/net/pstream>

#include <bscheduler/kernel/kernel_header.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_socket_pipeline.hh>

namespace bsc {

	template<class K, class R>
	class process_pipeline: public basic_socket_pipeline<K> {

	private:
		typedef basic_socket_pipeline<K> base_pipeline;
		using typename base_pipeline::queue_popper;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::mutex_type;
		typedef std::map<application_type,event_handler_ptr> map_type;
		typedef typename map_type::iterator app_iterator;

	public:
		typedef R router_type;
		typedef K kernel_type;

	private:
		map_type _apps;
		sys::process_group _procs;
		/// Allow process execution as superuser/supergroup.
		bool _allowroot = false;

	public:

		process_pipeline() = default;

		process_pipeline(const process_pipeline&) = delete;

		process_pipeline&
		operator=(const process_pipeline&) = delete;

		process_pipeline(process_pipeline&& rhs) = default;

		~process_pipeline() = default;

		inline void
		add(const application& app) {
			lock_type lock(this->_mutex);
			this->do_add(app);
		}

		void
		do_run() override;

		void
		forward(kernel_header& hdr, sys::pstream& istr);

		inline void
		allow_root(bool rhs) noexcept {
			this->_allowroot = rhs;
		}

	private:

		app_iterator
		do_add(const application& app);

		void
		process_kernel(kernel_type* k);

		void
		wait_for_all_processes_to_finish();

		void
		on_process_exit(const sys::process& p, sys::proc_info status);

		inline app_iterator
		find_by_app_id(application_type id) {
			return this->_apps.find(id);
		}

		app_iterator
		find_by_process_id(sys::pid_type pid);

		template <class X, class Y> friend class process_notify_handler;
		template <class X, class Y> friend class process_handler;

	};

}

#endif // vim:filetype=cpp
