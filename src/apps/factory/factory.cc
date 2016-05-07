#include "security.hh"
#include <sys/network_format.hh>
#include <sys/log.hh>

void
proceed_with_security_checks_or_exit() {
	factory::do_not_allow_to_run_as_superuser_or_setuid();
	factory::do_not_allow_ld_preload();
	factory::shred_environment();
	factory::ignore_all_signals_except_terminate();
	factory::close_all_file_descriptors();
}

int
bail_out() {
	std::cerr << "The environment has not passed minimal security checks. Exiting." << std::endl;
	return 1;
}

int main() {
	try {
		proceed_with_security_checks_or_exit();
	} catch (sys::bits::bad_call& err) {
		std::cerr << err << std::endl;
		return bail_out();
	} catch (std::exception& err) {
		std::cerr << err.what() << std::endl;
		return bail_out();
	}
	sys::endiannes_guard g1;
	sys::syslog_guard g2(std::clog, sys::syslog_guard::tee);
	return 0;
}
