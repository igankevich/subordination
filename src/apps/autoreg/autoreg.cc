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


typedef float T;
typedef REAL_TYPE Real;


class Autoreg_app: public Kernel {
public:

	Autoreg_app(int argc, char** argv):
		model_filename("autoreg.model"),
		homogeneous(true)
	{
		std::stringstream ln;
		for (int i=0; i<argc; i++)
			ln << argv[i] << ' ';
		cmdline(ln);
	}

	bool is_profiled() const { return false; }

	const char* name() const { return "A_1"; }

	void cmdline(istream& in) {
		std::string arg;
		while (!(in >> arg).eof()) {
			if (arg == "--homogeneous-model") homogeneous = true;
			else if (arg == "--heterogeneous-model") homogeneous = false;
		}
	}

	void act() {
		Autoreg_model<Real>* model = new Autoreg_model<Real>(homogeneous);
		try {
			std::ifstream cfg(model_filename.c_str());
			cfg >> *model;
		} catch (std::exception& e) {
			std::cerr << e.what();
			exit(1);
		}
		upstream(the_server(), model);
	}

	void react(factory::Kernel*) {
		commit(the_server());
	}

private:
	std::string model_filename;
	bool homogeneous;
};

Server* heterogeneous_computer() {
	std::clog << std::setw(20) << std::left << "Case: heterogeneous_computer" << std::endl;
	typedef Stochastic_round_robin U;
	typedef Simple_hashing D;
	std::size_t cpu_id = 0;
	return new Iserver<U, D>({
		new Iserver<U, D>(server_array<U>(std::max(1, total_threads()-1), cpu_id, total_vthreads())),
		new Rserver<U>(cpu_id)
	});
	cpu_id += total_vthreads();
}

Server* tetris(std::size_t cpu_id = 0) {
	std::clog << std::setw(20) << std::left << "Case: tetris" << std::endl;
	typedef Tetris U;
	typedef Simple_hashing D;
	return new Iserver<U, D>(server_array<U>(total_threads(), cpu_id, total_vthreads()));
}

Server* homogeneous_computer(std::size_t cpu_id = 0) {
	std::clog << std::setw(20) << std::left << "Case: homogeneous_computer" << std::endl;
	typedef Stochastic_round_robin U;
	typedef Simple_hashing D;
	return new Iserver<U, D>(server_array<U>(total_threads(), cpu_id, total_vthreads()));
}

Server* round_robin(std::size_t cpu_id = 0) {
	std::clog << std::setw(20) << std::left << "Case: homogeneous_computer (Round-robin)" << std::endl;
	typedef Round_robin U;
	typedef Simple_hashing D;
	return new Iserver<U, D>(server_array<U>(total_threads(), cpu_id, total_vthreads()));
}

int main(int argc, char** argv) {
	log_user_event("");

	std::stringstream cmdline;
	for (int i=0; i<argc; i++)
		cmdline << argv[i] << ' ';

	Server* root = nullptr;
	Server* downstream = nullptr;
	Server* io = nullptr;
	std::string arg;
	while (!(cmdline >> arg).eof()) {
		if (arg == "--homogeneous-computer") {
			root = homogeneous_computer();
			downstream = homogeneous_computer();
			io = new Rserver<Round_robin>(total_threads()-1);
		} else if (arg == "--heterogeneous-computer") {
			root = heterogeneous_computer();
		} else if (arg == "--tetris") {
			root = tetris();
//			downstream = tetris();
			io = new Rserver<Round_robin>(total_threads()-1);
		} else if (arg == "--round-robin") {
			root = round_robin();
			io = new Rserver<Round_robin>(total_threads()-1);
		}
		cmdline >> std::ws;
	}

	if (root == nullptr) {
		root = round_robin();
		downstream = round_robin();
		io = new Rserver<Round_robin>(total_threads()-1);
	}
	construct(root, downstream, io);
	std::clog << *the_server() << std::endl;
	the_server()->send(new Autoreg_app(argc, argv));
	the_server()->wait();
	log_user_event("");
	return 0;
}
