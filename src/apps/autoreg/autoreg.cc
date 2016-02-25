#include <iostream>
#include <fstream>
#include <sstream>
#include <valarray>
#include <limits>
#include <cmath>
#include <complex>
#include <ostream>
#include <functional>
#include <algorithm>
#include <numeric>
#include <stdexcept>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sysx::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;

#include "mapreduce.hh"
#include "mersenne.hh"
#include "ssysv.hh"
#include "domain.hh"
#include "spectrum.hh"
#include "autoreg.hh"
#include "nonlinear_omp.hh"
#include "fourier.hh"
#include "pressure.hh"
#include "autoreg_driver.hh"
using namespace autoreg;


typedef float Real;


struct Autoreg_app: public Kernel {

	template<class XXX>
	Autoreg_app(XXX&, int, char**):
	model_filename("autoreg.model")
	{}

	void act() {
		Autoreg_model<Real>* model = new Autoreg_model<Real>(true);
		try {
			std::ifstream cfg(model_filename.c_str());
			cfg >> *model;
		} catch (std::exception& e) {
			std::cerr << e.what();
			exit(1);
		}
		upstream(local_server(), model);
	}

	void react(factory::Kernel*) {
		commit(local_server());
	}

private:

	std::string model_filename;

};


int
main(int argc, char** argv) {
	return factory_main<Autoreg_app,config>(argc, argv);
}
