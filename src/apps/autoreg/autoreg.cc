#include <factory/api.hh>
#include <factory/base/error_handler.hh>

using namespace factory::api;

#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	factory::install_error_handler();
	Factory_guard g;
	send<Local>(new Autoreg_app);
	return factory::wait_and_return();
}
