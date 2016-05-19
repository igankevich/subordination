#ifndef APPS_AUTOREG_AUTOREG_APP_HH
#define APPS_AUTOREG_AUTOREG_APP_HH

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


#define DISABLE_RANDOM_SEED
#include "mapreduce.hh"
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

	Autoreg_app():
	model_filename("autoreg.model")
	{}

	template<class XXX>
	Autoreg_app(XXX&, int, char**):
	Autoreg_app()
	{}

	void act() {
		Autoreg_model<Real>* model = new Autoreg_model<Real>(true);
		try {
			std::ifstream cfg(model_filename.c_str());
			if (cfg.is_open()) {
				cfg >> *model;
			} else {
				#ifndef NDEBUG
				stdx::debug_message("autoreg", "using default parameters");
				#endif
			}
		} catch (std::exception& e) {
			std::cerr << e.what();
			exit(1);
		}
		factory::upstream(local_server, this, model);
	}

	void react(factory::Kernel*) {
		#ifndef NDEBUG
		stdx::debug_message("autoreg", "finished all");
		#endif
		factory::commit(local_server, this);
	}

private:

	std::string model_filename;

};

#endif // APPS_AUTOREG_AUTOREG_APP_HH
