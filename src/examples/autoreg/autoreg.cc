#include <factory/api.hh>
#include <factory/base/error_handler.hh>

using namespace factory::api;

#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	factory::install_error_handler();
	factory::types.register_type<autoreg::Autoreg_model<Real>>();
	factory::types.register_type<autoreg::Generator1<Real,autoreg::Uniform_grid>>();
	factory::types.register_type<autoreg::Wave_surface_generator<Real,autoreg::Uniform_grid>>();
	Factory_guard g;
	send<Local>(new Autoreg_app);
	return factory::wait_and_return();
}
