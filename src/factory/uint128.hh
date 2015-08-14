#include "bits/uint128.hh"

// prefer builtin types over substitute implementation
#if defined(HAVE___UINT128_T) && !defined(FACTORY_FORCE_CUSTOM_UINT128)
typedef __uint128_t uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
}
#else
typedef factory::bits::uint128 uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
}
#endif

// Attempt to simulate useful behaviour for __uint128_t
// (input/output operators, static type information.)
#ifdef HAVE___UINT128_T
namespace std {
	std::ostream& operator<<(std::ostream& o, __uint128_t rhs);
	std::istream& operator>>(std::istream& in, __uint128_t& rhs);
}
#endif

namespace std {

	template<> struct is_integral<uint128_t> {
		static const bool value = true;
	};

	template<> struct is_unsigned<uint128_t> {
		static const bool value = true;
	};

}

namespace factory {

	template<char ... Chars>
	constexpr uint128_t
	operator"" _u128() noexcept {
		return ::factory::bits::parse_uint<uint128_t, sizeof...(Chars)>((const char[]){Chars...});
	}

}
#define UINT128_C(x) x##_u128
