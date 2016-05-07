
#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/io_server.hh>
#include <factory/server/nic_server.hh>
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

	components::NIC_server<Kernel, sys::socket, Router> remote_server;

	void
	Router::send_remote(Kernel* rhs) {
		remote_server.send(rhs);
	}

}

using namespace factory;

#include "spec_app.hh"

int
main(int argc, char** argv) {
	local_server.send(new Spec_app);
	return wait_and_return();
}
