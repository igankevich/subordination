#include <unistdx/net/endian>
#include <factory/api.hh>

#include "network_master.hh"

int main() {
	using namespace factory::api;
	{ sys::endian_guard g1; }
	Factory_guard g;
	send<Local>(new factoryd::network_master);
	return factory::wait_and_return();
}
