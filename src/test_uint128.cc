#define FACTORY_FORCE_CUSTOM_UINT128
#include <factory/factory.hh>

using namespace factory;

void check_bool(bool x, bool y) {
	if (x != y) {
		throw std::runtime_error("Boolean operator failed.");
	}
}

template<class I>
void check_op(I x, const char* y) {
	std::stringstream tmp;
	tmp << x;
	std::string res = tmp.str();
	if (res != y) {
		std::stringstream msg;
		msg << "Result does not match the expected value.\nResult=  '" << res;
		msg << "',\nexpected='" << y << "'.";
		throw std::runtime_error(msg.str());
	}
}

template<class T>
void check_write(T rhs, const char* expected_result) {
	std::stringstream str;
	str << rhs;
	std::string result = str.str();
	if (result != expected_result) {
		std::stringstream msg;
		msg << "Write failed: result='" << result
			<< "', expected result='" << expected_result
			<< "'.";
		throw std::runtime_error(msg.str());
	}
}

void test_uint128() {
	// check different numbers
	check_op(uint128(0), "0");
	check_op(uint128(1), "1");
	check_op(uint128("100000000000000000000"), "100000000000000000000");
	check_op(uint128("18446744073709551615"), "18446744073709551615"); // 2^64-1
	check_op(uint128("18446744073709551616"), "18446744073709551616"); // 2^64
	check_op(uint128("18446744073709551617"), "18446744073709551617"); // 2^64+1
	check_op(uint128("18446744073709551618"), "18446744073709551618"); // 2^64+2
	check_op(uint128("1180591620717411303423"), "1180591620717411303423"); // 2^70-1
	check_op(uint128("1180591620717411303424"), "1180591620717411303424"); // 2^70
	check_op(uint128("1208925819614629174706176"), "1208925819614629174706176"); // 2^80
	check_op(uint128("1267650600228229401496703205376"), "1267650600228229401496703205376"); // 2^100
	check_op(uint128("170141183460469231731687303715884105727"), "170141183460469231731687303715884105727");
	check_op(uint128("340282366920938463463374607431768211455"), "340282366920938463463374607431768211455");
	check_op(uint128("0xffffffffffffffffffffffffffffffff"), "340282366920938463463374607431768211455");
	// check arithmetic operations
	check_op(uint128(1)+1, "2");
	check_op(uint128("18446744073709551615")+1, "18446744073709551616");
}

#ifdef HAVE___UINT128_T
void test___uint128_t() {
	check_write(__uint128_t(123), "123");
}
#else
void test___uint128_t() {}
#endif

struct App {
	int run(int, char**) {
		try {
			test_uint128();
			test___uint128_t();
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
