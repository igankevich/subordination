#ifndef FACTORY_BITS_UINT128_HH
#define FACTORY_BITS_UINT128_HH

// prefer builtin types over substitute implementation
#if defined(__SIZEOF_INT128__) && !defined(FACTORY_FORCE_CUSTOM_UINT128)
	#define FACTORY_HAVE_UINT128_T
#endif

#include "uint128_parse.hh"

#if defined(FACTORY_HAVE_UINT128_T)
typedef unsigned __int128 uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
}

// Attempt to simulate useful behaviour for ``unsigned __int128''
// (input/output operators, static type information.)
namespace std {

	inline std::ostream&
	operator<<(std::ostream& out, unsigned __int128 rhs) {
		std::ostream::sentry s(out);
		if (s) {
			factory::bits::uint128_put(rhs, out);
		}
		return out;
	}

	inline std::istream&
	operator>>(std::istream& in, unsigned __int128& rhs) {
		std::istream::sentry s(in);
		if (s) {
			factory::bits::uint128_get(rhs, in);
		}
		return in;
	}

}
#else
#include "basic_uint.hh"
typedef factory::bits::basic_uint<uint64_t> uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
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
		return bits::parse_uint<uint128_t, sizeof...(Chars)>((const char[]){Chars...});
	}

}
#define UINT128_C(x) x##_u128

#endif // FACTORY_BITS_UINT128_HH
