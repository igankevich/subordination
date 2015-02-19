#include "factory/layer1.hh"
#include "factory/release_configuration.hh"
#include "factory/layer2.hh"
#include "factory/default_factory.hh"
#include "factory/layer3.hh"
#include "test_marshaling.hh"

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
