#include <factory/factory_base.hh>

using namespace factory;

#include "datum.hh"

void test_marshaling() {
	const size_t size = 20;
	std::vector<Datum> input(size);
	std::vector<Datum> output(size);

	Foreign_stream stream;
	for (size_t i=0; i<input.size(); ++i)
		stream << input[i];

	for (size_t i=0; i<input.size(); ++i)
		stream >> output[i];
	
	for (size_t i=0; i<input.size(); ++i)
		if (input[i] != output[i])
			throw std::runtime_error("Input and output does not match.");
}

struct Thing: public Mobile<Thing> {
	static void init_type(Type* t) {
		t->id(123);
		t->name("Thing");
	}
	void write(std::ostream& out) const {
		out << _data;
	}
	void read(std::istream& in) {
		in >> _data;
	}
private:
	Datum _data;
};

struct App {

	int run(int, char**) {
		try {
			test_marshaling();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}

};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
