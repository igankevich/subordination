#include <stdx/debug.hh>

#include <factory/factory.hh>
#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/nic_server.hh>
#include <factory/kernel.hh>

namespace factory {

	struct Router {

		void
		send_local(Kernel* rhs) {
			local_server.send(rhs);
		}

		void
		send_remote(Kernel*);

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
			assert(false);
		}

	};

	NIC_server<Kernel, sys::socket, Router> remote_server;

	void
	Router::send_remote(Kernel* rhs) {
		remote_server.send(rhs);
	}

}

using namespace factory;

#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	factory::Terminate_guard g00;
	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(remote_server)> g2(remote_server);
	factory::upstream(local_server, nullptr, new Autoreg_app);
	return factory::wait_and_return();
}
