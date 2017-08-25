#include <factory/factory.hh>
#include <factory/ppl/cpu_server.hh>
#include <factory/ppl/timer_server.hh>
#include <factory/ppl/io_server.hh>
#include <factory/ppl/nic_server.hh>
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
		forward(const Kernel_header& hdr, sys::pstream& istr) {
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

#include "spec_app.hh"

int
main(int argc, char** argv) {
	local_server.send(new Spec_app);
	return wait_and_return();
}
