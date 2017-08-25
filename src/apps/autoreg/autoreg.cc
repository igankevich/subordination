#include <factory/factory.hh>
#include <factory/kernel.hh>
#include <factory/ppl/cpu_server.hh>
#include <factory/ppl/nic_server.hh>
#include <factory/ppl/timer_server.hh>

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

#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	factory::install_error_handler();
	factory::start_all(local_server, remote_server);
	factory::upstream(local_server, nullptr, new Autoreg_app);
	int ret = factory::wait_and_return();
	factory::stop_all(local_server, remote_server);
	factory::wait_all(local_server, remote_server);
	return ret;
}
