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

	typedef stdx::log<Autoreg_app> this_log;

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

#endif // APPS_AUTOREG_AUTOREG_APP_HH
