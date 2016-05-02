#ifndef FACTORY_FACTORY_HH
#define FACTORY_FACTORY_HH

#if __cplusplus < 201103L
	#error Factory requires C++11 compiler.
#endif

#include <sys/proc/process.hh>
#include <sys/log.hh>
#include <sys/packetstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

#include <factory/server/cpu_server.hh>
#include <factory/server/io_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>
#include <factory/server_guard.hh>

namespace factory {

	Server_guard<components::CPU_server<Kernel>> local_server;
	Server_guard<components::Timer_server<Kernel>> timer_server;
	Server_guard<components::IO_server<Kernel>> io_server;
	components::Global_thread_context _globalcon;

	Server_guard<components::NIC_server<Kernel,sys::socket>> remote_server;
	Instances<Kernel> instances;
	Types<Type<Kernel>> types;

	/// Server API
	void
	shutdown() {
		this_log() << "SHUTDOWN" << std::endl;
		local_server.shutdown();
		remote_server.shutdown();
		io_server.shutdown();
		timer_server.shutdown();
		local_server.stop();
		remote_server.stop();
		io_server.stop();
		timer_server.stop();
	}

	inline void compute(Kernel* rhs) { local_server.send(rhs); }
	inline void spill(Kernel* rhs) { remote_server.send(rhs); }
	inline void input(Kernel* rhs) { io_server.send(rhs); }
	inline void output(Kernel* rhs) { io_server.send(rhs); }

	inline void schedule(Kernel* rhs) { timer_server.send(rhs); }

	inline void
	schedule_after(Kernel::Duration dur, Kernel* rhs) {
		rhs->after(dur);
		timer_server.send(rhs);
	}

//	inline void yield() { compute(this); }
	inline void collect(Kernel* rhs) {
		rhs->return_to_parent();
		spill(rhs);
	}

	inline void commit(Kernel* rhs, Result ret) {
		if (!rhs->parent()) {
			shutdown();
		} else {
			rhs->return_to_parent(ret);
			compute(rhs);
		}
	}

	inline void commit(Kernel* rhs) {
		Result ret = rhs->result();
		commit(rhs, ret == Result::undefined ? Result::success : ret);
	}

	/// @deprecated
	template<class S>
	void
	upstream(S& this_server, Principal* a) {
		a->parent(this);
		this_server->send(a);
	}

	/// @deprecated
	template<class S>
	void
	upstream_carry(S& this_server, Principal* a) {
		a->parent(this);
		a->setf(Flag::carries_parent);
		this_server->send(a);
	}

	/*
	/// @deprecated
	template<class S>
	void
	commit(S& srv, Result res = Result::success) {
		this->principal(_parent);
		this->result(res);
		srv->send(this);
	}
	*/

}

#endif // FACTORY_FACTORY_HH
