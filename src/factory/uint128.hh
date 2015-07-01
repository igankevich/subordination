/*
 * Copyright (c) 2008
 * Evan Teran
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appears in all copies and that both the
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the same name not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission. We make no representations about the
 * suitability this software for any purpose. It is provided "as is"
 * without express or implied warranty.
 */

/// The code was modified to incorporate ``constexpr''
/// and remove ``boost'' dependencies. The code was taken
/// from http://www.codef00.com/code/uint128.h

// prefer builtin types over substitute implementation
#if defined(HAVE___UINT128_T) && !defined(FACTORY_FORCE_CUSTOM_UINT128)
typedef __uint128_t uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
	template<>
	struct is_integral<uint128_t> {
		constexpr static const bool value = true;
	};
}
#elif defined(HAVE___INT128) && !defined(FACTORY_FORCE_CUSTOM_UINT128)
typedef unsigned __int128 uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
	template<>
	struct is_integral<uint128_t> {
		constexpr static const bool value = true;
	};
}
#else
struct uint128 {

	typedef uint64_t base_type;

	// constructors for all basic types
	constexpr uint128(): lo(0), hi(0) {}
	constexpr uint128(int value): lo(static_cast<base_type>(value)), hi(0) {}
	constexpr uint128(unsigned int value): lo(static_cast<base_type>(value)), hi(0) {}
	constexpr uint128(const uint128 &value): lo(value.lo), hi(value.hi) {}
	constexpr uint128(base_type value): lo(value), hi(0) {}
	constexpr uint128(base_type l, base_type h): lo(l), hi(h) {}

	uint128(const std::string &sz): lo(0), hi(0) {
		from_string(sz.begin(), sz.end());
	}

	uint128 &operator=(const uint128 &other) {
		if (&other != this) {
			lo = other.lo;
			hi = other.hi;
		}
		return *this;
	}

	// comparison operators
	constexpr bool operator==(const uint128 &o) const { return hi == o.hi && lo == o.lo; }
	constexpr bool operator<(const uint128 &o) const { return (hi == o.hi) ? lo < o.lo : hi < o.hi; }

	// derived comparison operators
	constexpr bool operator!=(const uint128& y) { return !operator==(y); }
	constexpr bool operator> (const uint128& y) { return y < *this; }
	constexpr bool operator<=(const uint128& y) { return !(y < *this); }
	constexpr bool operator>=(const uint128& y) { return !operator<(y); }

	// unary operators
    constexpr explicit operator bool() const { return hi != 0 || lo != 0; }
    constexpr bool operator!() const { return !operator bool(); }

    uint128 operator-() const {
		// standard 2's compliment negation
		return ~uint128(*this) + 1u;
	}

    constexpr uint128 operator~() const { return uint128(~lo, ~hi); }

    uint128& operator++() {
    	if (++lo == 0) {
			++hi;
		}
    	return *this;
	}

    uint128& operator--() {
    	if (lo-- == 0) { --hi; }
    	return *this;
	}

	// derived increment/decrement operators
    uint128 operator++(int) const { return ++uint128(*this); }
    uint128 operator--(int) const { return --uint128(*this); }

	// basic math operators
    uint128& operator+=(const uint128 &b) {
		const base_type old_lo = lo;

    	lo += b.lo;
    	hi += b.hi;

		if (lo < old_lo) {
			++hi;
		}

    	return *this;
	}

    uint128& operator-=(const uint128 &b) {
		// it happens to be way easier to write it
		// this way instead of make a subtraction algorithm
		return *this += -b;
    }

    uint128& operator*=(const uint128 &b) {

		// check for multiply by 0
		// result is always 0 :-P
		if (b == 0u) {
			lo = 0;
			hi = 0;
		// check we aren't multiplying by 1
		} else if (b != 1u) {

    		uint128 a(*this);
    		uint128 t = b;

    		lo = 0;
    		hi = 0;

    		for (unsigned int i=0; i<NBITS; ++i) {
        		if ((t & 1) != 0) {
            		*this += (a << i);
				}

        		t >>= 1;
    		}
		}

    	return *this;
	}

    uint128& operator|=(const uint128 &b) {
    	hi |= b.hi;
    	lo |= b.lo;
    	return *this;
	}

    uint128& operator&=(const uint128 &b) {
    	hi &= b.hi;
    	lo &= b.lo;
    	return *this;
	}

    uint128& operator^=(const uint128& b) {
    	hi ^= b.hi;
    	lo ^= b.lo;
    	return *this;
	}

    uint128& operator/=(const uint128& b) {
		uint128 remainder;
		divide(*this, b, *this, remainder);
		return *this;
    }

    uint128& operator%=(const uint128& b) {
		uint128 quotient;
		divide(*this, b, quotient, *this);
		return *this;
    }

    uint128& operator<<=(const uint128& rhs) {

		unsigned int n = rhs.to_integer();

		if(n >= NBITS) {
			hi = 0;
			lo = 0;
		} else {
			const unsigned int halfsize = NBITS / 2;

    		if(n >= halfsize){
        		n -= halfsize;
        		hi = lo;
        		lo = 0;
    		}

    		if(n != 0) {
				// shift high half
        		hi <<= n;

				const base_type mask(~(base_type(-1) >> n));

				// and add them to high half
        		hi |= (lo & mask) >> (halfsize - n);

				// and finally shift also low half
        		lo <<= n;
    		}
		}

    	return *this;
	}

    uint128 &operator>>=(const uint128& rhs) {

		unsigned int n = rhs.to_integer();

		if(n >= NBITS) {
			hi = 0;
			lo = 0;
		} else {
			const unsigned int halfsize = NBITS / 2;

    		if(n >= halfsize) {
        		n -= halfsize;
        		lo = hi;
        		hi = 0;
    		}

    		if(n != 0) {
				// shift low half
        		lo >>= n;

				// get lower N bits of high half
				const base_type mask(~(base_type(-1) << n));

				// and add them to low qword
        		lo |= (hi & mask) << (halfsize - n);

				// and finally shift also high half
        		hi >>= n;
    		}
		}

    	return *this;
	}

	// derived arithmetic operators
	uint128 operator+(const uint128& rhs) const { return uint128(*this) += rhs; }
	uint128 operator-(const uint128& rhs) const { return uint128(*this) -= rhs; }
	uint128 operator/(const uint128& rhs) const { return uint128(*this) /= rhs; }
	uint128 operator*(const uint128& rhs) const { return uint128(*this) *= rhs; }
	uint128 operator%(const uint128& rhs) const { return uint128(*this) %= rhs; }

	// derived bit-wise oeprators
	constexpr uint128 operator&(const uint128& rhs) const { return uint128(lo & rhs.lo, hi & rhs.hi); }
	constexpr uint128 operator|(const uint128& rhs) const { return uint128(lo | rhs.lo, hi | rhs.hi); }
	constexpr uint128 operator^(const uint128& rhs) const { return uint128(lo ^ rhs.lo, hi ^ rhs.hi); }
	uint128 operator<<(const uint128& rhs) const { return uint128(*this) <<= rhs; }
	uint128 operator>>(const uint128& rhs) const { return uint128(*this) >>= rhs; }

	constexpr int to_integer() const { return static_cast<int>(lo); }
	constexpr base_type to_base_type() const { return lo; }

    std::string to_string(unsigned int radix = 10) const {
    	if (*this == 0) {
			return "0";
		}

    	if (radix < 2 || radix > 37) {
			return "(invalid radix)";
		}

		// at worst it will be NBITS digits (base 2) so make our buffer
		// that plus room for null terminator
    	char buf[NBITS + 1];
		buf[sizeof(buf) - 1] = '\0';

    	uint128 ii(*this);
    	int i = NBITS - 1;

    	while (ii != 0 && i) {

			uint128 remainder;
			divide(ii, uint128(radix), ii, remainder);
        	buf[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[remainder.to_integer()];
    	}

    	return &buf[i];
	}

	friend std::ostream &operator<<(std::ostream &o, const uint128 &n) {
		switch(o.flags() & (std::ios_base::hex | std::ios_base::dec | std::ios_base::oct)) {
			case std::ios_base::hex: o << n.to_string(16); break;
			case std::ios_base::dec: o << n.to_string(10); break;
			case std::ios_base::oct: o << n.to_string(8); break;
			default: o << n.to_string(); break;
		}
		return o;
	}

	friend std::istream& operator>>(std::istream& in, uint128& rhs) {
		std::istream_iterator<char> it(in), eos;
		rhs.from_string(it, eos);
		return in;
	}

private:

	template<class It>
	void from_string(It first, It last) {
		// do we have at least one character?
		if (first == last) return;
		// make some reasonable assumptions
		unsigned int radix = 10;
		// check if there is radix changing prefix (0 or 0x)
		if (*first == '0') {
			radix = 8;
			++first;
			if (first != last) {
				if (*first == 'x') {
					radix = 16;
					++first;
				}
			}
		}

		while (first != last) {
			unsigned int n;
			const char ch = *first;
			if (ch >= 'A' && ch <= 'Z') {
				if (ch - 'A' + 10 < radix) {
					n = ch - 'A' + 10;
				} else {
					break;
				}
			} else if (ch >= 'a' && ch <= 'z') {
				if (ch - 'a' + 10 < radix) {
					n = ch - 'a' + 10;
				} else {
					break;
				}
			} else if (ch >= '0' && ch <= '9') {
				if (ch - '0' < radix) {
					n = ch - '0';
				} else {
					break;
				}
			} else {
				/* completely invalid character */
				break;
			}

			(*this) *= radix;
			(*this) += n;

			++first;
		}
	}

	static void divide(const uint128 &numerator, const uint128 &denominator,
		uint128 &quotient, uint128 &remainder)
	{
		if (denominator == 0) {
			throw std::domain_error("divide by zero");
		} else {
			uint128 n      = numerator;
			uint128 d      = denominator;
			uint128 x      = 1;
			uint128 answer = 0;
	
			while ((n >= d) && (((d >> (NBITS - 1)) & 1) == 0)) {
				x <<= 1;
				d <<= 1;
			}
	
			while (x != 0) {
				if(n >= d) {
					n -= d;
					answer |= x;
				}
	
				x >>= 1;
				d >>= 1;
			}
	
			quotient = answer;
			remainder = n;
		}
	}

	base_type lo;
	base_type hi;

	static const unsigned int NBITS = (sizeof(base_type) + sizeof(base_type))
		* std::numeric_limits<unsigned char>::digits;
};

typedef uint128 uint128_t;
namespace std {
	typedef ::uint128_t uint128_t;
}

// convinience macro
#define UINT128_C(s) uint128_t(#s)
#endif