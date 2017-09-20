#ifndef APPS_AUTOREG_AUTOREG_APP_HH
#define APPS_AUTOREG_AUTOREG_APP_HH

#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <valarray>


#define DISABLE_RANDOM_SEED
#include "autoreg.hh"
#include "autoreg_driver.hh"
#include "domain.hh"
#include "mapreduce.hh"
#include "ssysv.hh"
#include "valarray_ext.hh"
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

	void
	act() {
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

	void
	react(asc::Kernel*) {
		#ifndef NDEBUG
		sys::log_message("autoreg", "finished all");
		#endif
		asc::commit(this);
	}

private:

	std::string model_filename;

};

#endif // vim:filetype=cpp
