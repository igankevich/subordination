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
void check_write(T rhs, const char* expected_result, std::ios_base& ( *pf )(std::ios_base&) = nullptr) {
	std::stringstream str;
	if (pf) { str << pf; }
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

template<class T>
void check_read(const char* str, T expected_result, std::ios_base& ( *pf )(std::ios_base&) = nullptr) {
	T result;
	std::stringstream s;
	s << str;
	if (pf) s >> pf;
	s >> result;
	if (result != expected_result) {
		std::stringstream msg;
		msg << "Read failed for '" << str << "': '"
			<< result << "' (read) /= '" << expected_result << "' (expected).";
		throw std::runtime_error(msg.str());
	}
}

template<class E, class F>
void check_exception(F func) {
	bool caught = false;
	try {
		func();
	} catch (E& err) {
		caught = true;
	}
	if (!caught) {
		throw std::runtime_error("Expected exception was not caught.");
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
	check_op(uint128("0x0123456789abcdef"), "81985529216486895");
	check_write(uint128("0x123456789abcdef"), "123456789abcdef", std::hex);
	check_write(uint128("0123"), "123", std::oct);
	// check arithmetic operations
	check_op(uint128(1)+1, "2");
	check_op(uint128("18446744073709551615")+1, "18446744073709551616");
	check_op(uint128("18446744073709551615")*uint128("1000"), "18446744073709551615000");
	check_op(uint128("18446744073709551615")*uint128("0"), "0");
	check_op(uint128("18446744073709551615")*uint128("1"), "18446744073709551615");
	check_op(uint128("18446744073709551615")%uint128("1000"), "615");
	check_op(uint128("18446744073709551615")/uint128("1000"), "18446744073709551");
	check_op(uint128("18446744073709551615")-uint128("1000"), "18446744073709550615");
	// check logical operations
	check_op(uint128("18446744073709551615")&uint128("123456789"), "123456789");
	check_op(uint128("18446744073709551615")|uint128("123456789"), "18446744073709551615");
	check_op(uint128("18446744073709551615")^uint128("123456789"), "18446744073586094826");
	check_write(uint128("0xffffffffffffff")<<uint128("8"), "ffffffffffffff00", std::hex);
	check_write(uint128("0xffffffffffffff")>>uint128("8"), "ffffffffffff", std::hex);
	check_write(uint128("0xffffffffffffff")>>uint128("129"), "0", std::hex);
	check_write(uint128("0xffffffffffffff")<<uint128("129"), "0", std::hex);
	check_write(uint128("0xffffffffffffff")>>uint128("100"), "0", std::hex);
	check_write(uint128("0xffffffffffffff")<<uint128("100"), "fffffff0000000000000000000000000", std::hex);
	// check I/O
	check_write(uint128("123"), "123");
	check_write(uint128("0x123456789abcdef"), "123456789abcdef", std::hex);
	check_write(uint128("0123"), "123", std::oct);
	check_read("123", uint128("123"));
	check_read("123", uint128("0123"), std::oct);
	check_read("123456789abcdef", uint128("0x123456789abcdef"), std::hex);
	check_read("ffffffff", uint128("0xffffffff"), std::hex);
	check_read("abcdef", uint128("0xabcdef"), std::hex);
	// common errors
	check_exception<std::logic_error>([] () { uint128("1")/uint128("0"); });
}

#ifdef HAVE___UINT128_T
void test___uint128_t() {
	check_write(__uint128_t(123), "123");
	check_write(__uint128_t(0x123456789abcdef), "123456789abcdef", std::hex);
	check_write(__uint128_t(0123), "123", std::oct);
	check_read("123", __uint128_t(123));
	check_read("123", __uint128_t(0123), std::oct);
	check_read("123456789abcdef", __uint128_t(0x123456789abcdef), std::hex);
	check_read("ffffffff", __uint128_t(0xffffffff), std::hex);
	check_read("abcdef", __uint128_t(0xabcdef), std::hex);
	check_read("AbCdeF1234", __uint128_t(0xabcdef1234), std::hex);
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
