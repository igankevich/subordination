namespace factory {

	/// @addtogroup byteswap Byte swap
	/// @brief Compile-time byte swapping functions.
	/// @{
	template<class T>
	constexpr T byte_swap (T n) { return n; }
	
	template<>
	constexpr uint16_t byte_swap<uint16_t>(uint16_t n) {
		return ((n & 0xFF00)>>8) | ((n & 0x00FF)<<8);
	}
	
	template<>
	constexpr uint32_t byte_swap<uint32_t>(uint32_t n) {
#ifdef HAVE___BUILTIN_BSWAP32
		return __builtin_bswap32(n);
#else
		return
			((n & UINT32_C(0xff000000)) >> 24) |
			((n & UINT32_C(0x00ff0000)) >> 8) |
			((n & UINT32_C(0x0000ff00)) << 8) |
			((n & UINT32_C(0x000000ff)) << 24);
#endif
	}
	
	template<>
	constexpr uint64_t byte_swap<uint64_t>(uint64_t n) {
#ifdef HAVE___BUILTIN_BSWAP64
		return __builtin_bswap64(n);
#else
		return
			((n & UINT64_C(0xff00000000000000)) >> 56) |
			((n & UINT64_C(0x00ff000000000000)) >> 40) |
			((n & UINT64_C(0x0000ff0000000000)) >> 24) |
			((n & UINT64_C(0x000000ff00000000)) >> 8) |
			((n & UINT64_C(0x00000000ff000000)) << 8) |
			((n & UINT64_C(0x0000000000ff0000)) << 24) |
			((n & UINT64_C(0x000000000000ff00)) << 40) |
			((n & UINT64_C(0x00000000000000ff)) << 56);
#endif
	}
	template<>
	std::array<uint8_t,16> byte_swap(std::array<uint8_t,16> n) {
		std::reverse(n.begin(), n.end());
		return n;
	}
	/// @}

	constexpr bool is_network_byte_order() {
		return __BYTE_ORDER == __BIG_ENDIAN;
	}

	template<class T>
	constexpr T to_network_format(T n) {
		return is_network_byte_order() ? n : byte_swap<T>(n);
	}

	template<class T>
	constexpr T to_host_format(T n) { return to_network_format(n); }

	template<size_t bytes>
	struct Integral { typedef uint8_t type[bytes]; };
	template<> struct Integral<1> { typedef uint8_t type; };
	template<> struct Integral<2> { typedef uint16_t type; };
	template<> struct Integral<4> { typedef uint32_t type; };
	template<> struct Integral<8> { typedef uint64_t type; };
	template<> struct Integral<16> { typedef std::array<uint8_t,16> type; };

	template<class T, class Byte=char>
	union Bytes {

		typedef typename Integral<sizeof(T)>::type Int;
		typedef Bytes<T,Byte> This;
		typedef typename std::decay<Int>::type Retval;

		constexpr Bytes(): val{} {}
		constexpr Bytes(T v): val(v) {}
		template<class It>
		Bytes(It first, It last) { std::copy(first, last, bytes); }

		Retval to_network_format() { return i = factory::to_network_format(i); }
		Retval to_host_format() { return i = factory::to_host_format(i); }

		operator T& () { return val; }
		operator Byte* () { return bytes; }
		constexpr operator const T& () const { return val; }
		constexpr operator const Byte* () const { return bytes; }

		constexpr Byte operator[](size_t idx) const { return bytes[idx]; }
		constexpr bool operator==(const Bytes<T>& rhs) const { return i == rhs.i; }
		constexpr bool operator!=(const Bytes<T>& rhs) const { return i != rhs.i; }

		Byte* begin() { return bytes; }
		Byte* end() { return bytes + sizeof(T); }

		constexpr const Byte* begin() const { return bytes; }
		constexpr const Byte* end() const { return bytes + sizeof(T); }

		constexpr const T& value() const { return val; }
		T& value() { return val; }

		constexpr size_t size() const { return sizeof(bytes); }

	private:
		T val;
		Int i;
		Byte bytes[sizeof(T)];

		static_assert(sizeof(decltype(val)) == sizeof(decltype(i)),
			"Bad size of integral type.");
	};

	template<class T, class B>
	std::ostream& operator<<(std::ostream& out, const Bytes<T,B>& rhs) {
		out << std::hex << std::setfill('0');
		std::ostream_iterator<unsigned int> it(out, " ");
		std::for_each(rhs.begin(), rhs.end(), [&out] (char ch) {
			out << (unsigned int)(unsigned char)ch << ' ';
		});
//		std::copy(rhs.begin(), rhs.end(), it);
		out << std::dec << std::setfill(' ');
		return out;
//		return out << static_cast<const T&>(rhs);
	}

	template<class T, class B>
	std::istream& operator>>(std::istream& in, Bytes<T,B>& rhs) {
		return in >> static_cast<T&>(rhs);
	}

}
