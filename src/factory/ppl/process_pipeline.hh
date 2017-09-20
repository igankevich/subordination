#ifndef FACTORY_PPL_PROCESS_PIPELINE_HH
#define FACTORY_PPL_PROCESS_PIPELINE_HH

#include <map>
#include <memory>

#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/net/pstream>

#include <factory/kernel/kernel_header.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/process_handler.hh>

namespace asc {

	template<class K, class R>
	class process_pipeline:
		public basic_socket_pipeline<K,process_handler<K,R>> {

	private:
		typedef basic_socket_pipeline<K,process_handler<K, R>> base_pipeline;
		using typename base_pipeline::queue_popper;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::lock_type;
		typedef std::map<application_type,event_handler_ptr> map_type;
		typedef typename map_type::iterator app_iterator;

	public:
		typedef R router_type;
		typedef K kernel_type;

	private:
		map_type _apps;
		sys::process_group _procs;

	public:

		process_pipeline() = default;

		process_pipeline(const process_pipeline&) = delete;

		process_pipeline&
		operator=(const process_pipeline&) = delete;

		process_pipeline(process_pipeline&& rhs) = default;

		~process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override;

		void
		process_kernels() override;

		void
		add(const application& app);

		void
		do_run() override;

		void
		forward(const kernel_header& hdr, sys::pstream& istr);

	private:

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

	};

}

#endif // vim:filetype=cpp
