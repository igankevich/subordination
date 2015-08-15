#include <factory/factory_base.hh>
#include "libfactory.cc"
#include "test.hh"

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
	check_op(uint128_t(0u), "0");
	check_op(uint128_t(1), "1");
	check_op(100000000000000000000_u128, "100000000000000000000");
	check_op(18446744073709551615_u128, "18446744073709551615"); // 2^64-1
	check_op(18446744073709551616_u128, "18446744073709551616"); // 2^64
	check_op(18446744073709551617_u128, "18446744073709551617"); // 2^64+1
	check_op(18446744073709551618_u128, "18446744073709551618"); // 2^64+2
	check_op(1180591620717411303423_u128, "1180591620717411303423"); // 2^70-1
	check_op(1180591620717411303424_u128, "1180591620717411303424"); // 2^70
	check_op(1208925819614629174706176_u128, "1208925819614629174706176"); // 2^80
	check_op(1267650600228229401496703205376_u128, "1267650600228229401496703205376"); // 2^100
	check_op(170141183460469231731687303715884105727_u128, "170141183460469231731687303715884105727");
	check_op(340282366920938463463374607431768211455_u128, "340282366920938463463374607431768211455");
	check_op(0xffffffffffffffffffffffffffffffff_u128, "340282366920938463463374607431768211455");
	check_op(0x0123456789abcdef_u128, "81985529216486895");
	check_write(0x123456789abcdef_u128, "123456789abcdef", std::hex);
	check_write(0123_u128, "123", std::oct);
	// check arithmetic operations
	check_op(uint128_t(1)+1, "2");
	check_op(18446744073709551615_u128+1, "18446744073709551616");
	check_op(18446744073709551615_u128*1000_u128, "18446744073709551615000");
	check_op(18446744073709551615_u128*0_u128, "0");
	check_op(18446744073709551615_u128*1_u128, "18446744073709551615");
	check_op(18446744073709551615_u128%1000_u128, "615");
	check_op(18446744073709551615_u128/1000_u128, "18446744073709551");
	check_op(18446744073709551615_u128-1000_u128, "18446744073709550615");
	// check logical operations
	check_op(18446744073709551615_u128&123456789_u128, "123456789");
	check_op(18446744073709551615_u128|123456789_u128, "18446744073709551615");
	check_op(18446744073709551615_u128^123456789_u128, "18446744073586094826");
	check_write(0xffffffffffffff_u128<<8_u128, "ffffffffffffff00", std::hex);
	check_write(0xffffffffffffff_u128>>8_u128, "ffffffffffff", std::hex);
//	check_write(0xffffffffffffff_u128>>129_u128, "0", std::hex);
//	check_write(0xffffffffffffff_u128<<129_u128, "0", std::hex);
//	check_write(0xffffffffffffff_u128>>100_u128, "0", std::hex);
	check_write(0xffffffffffffff_u128<<100_u128, "fffffff0000000000000000000000000", std::hex);
	// check I/O
	check_write(123_u128, "123");
	check_write(0x123456789abcdef_u128, "123456789abcdef", std::hex);
	check_write(0123_u128, "123", std::oct);
	check_read("123", 123_u128);
	check_read("123", 0123_u128, std::oct);
	check_read("123456789abcdef", 0x123456789abcdef_u128, std::hex);
	check_read("ffffffff", 0xffffffff_u128, std::hex);
	check_read("abcdef", 0xabcdef_u128, std::hex);
	// common errors
//	check_exception<std::logic_error>([] () { uint128_t(1)/uint128_t(0); });
	// literals
	check_write(10000_u128, "10000");
	check_write(0xabcde_u128, "abcde", std::hex);
	check_write(010000_u128, "10000", std::oct);
}

#ifdef HAVE___UINT128_T

	template<class T>
	inline T
	byte_swap2(T x) noexcept {
		int i = sizeof(x) * std::numeric_limits<unsigned char>::digits / 2;
		T k = (T(1) << i) - 1;
		while (i >= 8)
		{
		    x = ((x & k) << i) ^ ((x >> i) & k);
		    i >>= 1;
		    k ^= k << i;
		}
		return k;
	}
	template<class T>
	inline T
	byte_swap3(T n) noexcept {
		return
			((n & UINT128_C(0xff000000000000000000000000000000)) >> 120) |
			((n & UINT128_C(0x00ff0000000000000000000000000000)) >> 104) |
			((n & UINT128_C(0x0000ff00000000000000000000000000)) >> 88)  |
			((n & UINT128_C(0x000000ff000000000000000000000000)) >> 72)  |
			((n & UINT128_C(0x00000000ff0000000000000000000000)) >> 56)  |
			((n & UINT128_C(0x0000000000ff00000000000000000000)) >> 40)  |
			((n & UINT128_C(0x000000000000ff000000000000000000)) >> 24)  |
			((n & UINT128_C(0x00000000000000ff0000000000000000)) >> 8)   |
			((n & UINT128_C(0x0000000000000000ff00000000000000)) << 8)   |
			((n & UINT128_C(0x000000000000000000ff000000000000)) << 24)  |
			((n & UINT128_C(0x00000000000000000000ff0000000000)) << 40)  |
			((n & UINT128_C(0x0000000000000000000000ff00000000)) << 56)  |
			((n & UINT128_C(0x000000000000000000000000ff000000)) << 72)  |
			((n & UINT128_C(0x00000000000000000000000000ff0000)) << 88)  |
			((n & UINT128_C(0x0000000000000000000000000000ff00)) << 104) |
			((n & UINT128_C(0x000000000000000000000000000000ff)) << 120);
	}

	std::random_device rng;
template<class T>
void test_bytes_swap2_perf() {
	Time t0 = current_time_nano();
	for (int i=0; i<100000; ++i) {
		T x = stdx::n_random_bytes<T>(rng);
		T y = byte_swap3<T>(x);
	}
	Time t1 = current_time_nano();
	std::cout << "byte_swap3: time=" << (t1-t0) << "ns" << std::endl;
}
template<class T>
void test_bytes_swap_perf() {
	Time t0 = current_time_nano();
	for (int i=0; i<100000; ++i) {
		T x = stdx::n_random_bytes<T>(rng);
		T y = bits::byte_swap<T>(x);
	}
	Time t1 = current_time_nano();
	std::cout << "byte_swap: time=" << (t1-t0) << "ns" << std::endl;
}
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
	check_write(10000_u128, "10000");
	check_write(0xabcde_u128, "abcde", std::hex);
	check_write(010000_u128, "10000", std::oct);
//	test_bytes_swap_perf<__uint128_t>();
//	test_bytes_swap2_perf<__uint128_t>();
//	test_bytes_swap_perf<uint128_t>();
//	test_bytes_swap2_perf<uint128_t>();
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
