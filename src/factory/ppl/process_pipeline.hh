#ifndef FACTORY_PPL_PROCESS_PIPELINE_HH
#define FACTORY_PPL_PROCESS_PIPELINE_HH

#include <map>
#include <memory>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/queue_popper>
#include <unistdx/net/pstream>

#include <factory/ppl/application.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/process_handler.hh>
#include <factory/ppl/shared_fildes.hh>

namespace factory {

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

}

#endif // vim:filetype=cpp
