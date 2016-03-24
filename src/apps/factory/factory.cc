#include "security.hh"

void
proceed_with_security_checks_or_exit() {
	factory::do_not_allow_to_run_as_superuser_or_setuid();
	factory::do_not_allow_ld_preload();
	factory::shred_environment();
	factory::ignore_all_signals_except_terminate();
	factory::close_all_file_descriptors();
}

int main() {
	try {
		proceed_with_security_checks_or_exit();
	} catch (sysx::bits::bad_call& err) {
		std::cerr << err << std::endl;
		std::cerr << "The environment has not passed minimal security checks. Exiting." << std::endl;
	} catch (std::exception& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << "The environment has not passed minimal security checks. Exiting." << std::endl;
		return 1;
	}
	return 0;
}
