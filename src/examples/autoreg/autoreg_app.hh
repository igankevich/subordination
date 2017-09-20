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
#include "autoreg.hh"
#include "valarray_ext.hh"
#include "autoreg_driver.hh"
using namespace autoreg;


typedef float Real;

struct Autoreg_app: public asc::Kernel {

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
				sys::log_message("autoreg", "using default parameters");
				#endif
			}
		} catch (const std::exception& e) {
			std::cerr << e.what();
			exit(1);
		}
		asc::upstream(this, model);
	}

	void react(asc::Kernel*) {
		#ifndef NDEBUG
		sys::log_message("autoreg", "finished all");
		#endif
		asc::commit(this);
	}

private:

	std::string model_filename;

};

#endif // vim:filetype=cpp
