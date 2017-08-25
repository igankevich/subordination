#include "basic_server.hh"

namespace {

	std::promise<int> return_value;

}

void
factory::graceful_shutdown(int ret) {
	return_value.set_value(ret);
}

int
factory::wait_and_return() {
	return return_value.get_future().get();
}
