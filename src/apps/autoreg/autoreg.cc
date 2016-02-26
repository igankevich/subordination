#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	return factory_main<Autoreg_app,config>(argc, argv);
}
