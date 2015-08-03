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
}
#else
struct uint128 {

	typedef uint64_t base_type;

	// constructors for all basic types
	uint128() = default;
	uint128(const uint128& rhs) = default;
	constexpr uint128(int value): lo(static_cast<base_type>(value)), hi(0) {}
	constexpr uint128(unsigned int value): lo(static_cast<base_type>(value)), hi(0) {}
	constexpr uint128(base_type value): lo(value), hi(0) {}
	constexpr uint128(base_type l, base_type h): lo(l), hi(h) {}

	uint128(const std::string &sz): lo(0), hi(0) {
		from_string(sz.begin(), sz.end());
	}

	uint128(const char* sz): lo(0), hi(0) {
		from_string(sz, sz + std::strlen(sz));
	}

	uint128& operator=(const uint128& other) {
		if (&other != this) {
			lo = other.lo;
			hi = other.hi;
		}
		return *this;
	}

	// comparison operators
	constexpr bool operator==(const uint128 &o) const { return hi == o.hi && lo == o.lo; }
	constexpr bool operator==(unsigned int rhs) const { return hi == 0 && lo == rhs; }
	constexpr bool operator<(const uint128 &o) const { return (hi == o.hi) ? lo < o.lo : hi < o.hi; }

	// derived comparison operators
	constexpr bool operator!=(const uint128& y) const { return !operator==(y); }
	constexpr bool operator!=(unsigned int rhs) const { return !operator==(rhs); }
	constexpr bool operator> (const uint128& y) const { return y < *this; }
	constexpr bool operator<=(const uint128& y) const { return !(y < *this); }
	constexpr bool operator>=(const uint128& y) const { return !operator<(y); }

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
        		if (t & 1) {
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

	friend std::ostream &operator<<(std::ostream& out, const uint128& n) {
		std::ostream::sentry s(out);
		if (s) {
			int radix = uint128::get_radix(out);
	    	if (n == 0) { return out << '0'; }
			// at worst it will be NBITS digits (base 2) so make our buffer
			// that plus room for null terminator
	    	char buf[NBITS + 1] = {};
//			buf[sizeof(buf) - 1] = '\0';
	
	    	uint128 ii(n);
	    	int i = NBITS - 1;
	
	    	while (ii != 0 && i) {
				uint128 remainder;
				divide(ii, uint128(radix), ii, remainder);
	        	buf[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[remainder.to_integer()];
	    	}
	
	    	out << buf + i;
		}
		return out;
	}

	friend std::istream& operator>>(std::istream& in, uint128& rhs) {
		std::istream::sentry s(in);
		if (s) {
			rhs = 0;
			unsigned int radix = uint128::get_radix(in);
			char ch;
			while (in >> ch) {
				unsigned int n = radix;
				switch (radix) {
					case 16: {
						ch = std::tolower(ch);
						if (ch >= 'a' && ch <= 'f') {
							n = ch - 'a' + 10;
						} else if (ch >= '0' && ch <= '9') {
							n = ch - '0';
						}
					} break;
					case 8:	
						if (ch >= '0' && ch <= '7') {
							n = ch - '0';
						}
						break;
					case 10:
					default:
						if (ch >= '0' && ch <= '9') {
							n = ch - '0';
						}
						break;
				}
				if (n == radix) {
					break;
				}

				rhs *= radix;
				rhs += n;
			}
		}
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
			const char ch = std::tolower(*first);
			if (ch >= 'a' && ch <= 'z') {
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

	static int get_radix(const std::ios_base& str) {
		switch (str.flags() & (std::ios_base::hex | std::ios_base::dec | std::ios_base::oct)) {
			case std::ios_base::hex: return 16;
			case std::ios_base::oct: return 8;
			case std::ios_base::dec:
			default: return 10;
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
#endif

// Attempt to simulate useful behaviour for __uint128_t
// (input/output operators, static type information.)
#ifdef HAVE___UINT128_T
namespace std {
	template<> struct is_integral<__uint128_t> {
		static const bool value = true;
	};
	inline
	int uint128_get_radix(const std::ios_base& out) {
		return out.flags() & std::ios_base::hex ? 16 :
			out.flags() & std::ios_base::oct ? 8 : 10;
	}
	std::ostream& operator<<(std::ostream& o, __uint128_t rhs);
	std::istream& operator>>(std::istream& in, __uint128_t& rhs);
}
#endif
