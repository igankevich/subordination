#include <factory/api.hh>
#include <factory/base/error_handler.hh>

using namespace factory;

#include "spec_app.hh"

int
main(int argc, char** argv) {
	using namespace factory::api;
	install_error_handler();
	Factory_guard g;
	send<Local>(new Spec_app);
	return wait_and_return();
}
