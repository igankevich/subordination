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
	/// TODO: rewrite with compile-time user-defined literarals (as in the code above).
	template<>
	inline
	uint128_t byte_swap(uint128_t n) {
		union {
			uint128_t x;
			unsigned char y[sizeof(uint128_t)];
		} tmp;
		tmp.x = n;
		std::reverse(tmp.y, tmp.y + sizeof(tmp.y));
		return n;
	}

	// compile-time unit tests for byte swapping
	static_assert(byte_swap<uint16_t>(UINT16_C(0xABCD)) == UINT16_C(0xCDAB), "byte swap failed for u16");
	static_assert(byte_swap<uint32_t>(UINT32_C(0xABCDDCBA)) == UINT32_C(0xBADCCDAB), "byte swap failed for u32");
	static_assert(byte_swap<uint64_t>(UINT64_C(0xABCDDCBA12344321)) == UINT64_C(0x21433412BADCCDAB), "byte swap failed for u64");
	/// @}

#if defined(WORDS_BIGENDIAN)
	constexpr bool is_network_byte_order() { return true; }
#else
	constexpr bool is_network_byte_order() { return false; }
#endif
/*
	constexpr bool is_network_byte_order() {
		return __BYTE_ORDER == __BIG_ENDIAN;
	}
*/

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
	template<> struct Integral<16> { typedef uint128_t type; };

	template<class T, class Byte=char>
	union Bytes {

		typedef typename Integral<sizeof(T)>::type Int;
		typedef Bytes<T,Byte> This;
		typedef typename std::decay<Int>::type Retval;

		constexpr Bytes(): val{} {}
		constexpr Bytes(const Bytes& rhs): val(rhs.val) {}
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

		constexpr size_t size() const { return sizeof(T); }

	private:
		T val;
		Int i;
		Byte bytes[sizeof(T)];

		static_assert(sizeof(decltype(val)) == sizeof(decltype(i)),
			"Bad size of integral type.");
	};

	template<class T>
	constexpr Bytes<T> make_bytes(T rhs) { return Bytes<T>(rhs); }

	template<class T, class B>
	std::ostream& operator<<(std::ostream& out, const Bytes<T,B>& rhs) {
		std::ostream::sentry s(out);
		if (s) {
			std::ostream::fmtflags oldf = out.flags();
			out << std::hex << std::setfill('0');
			std::ostream_iterator<unsigned int> it(out, " ");
			std::for_each(rhs.begin(), rhs.end(), [&out] (char ch) {
				out << std::setw(2)
					<< (unsigned int)(unsigned char)ch << ' ';
			});
			out.setf(oldf);
		}
		return out;
	}

	template<class T, class B>
	std::istream& operator>>(std::istream& in, Bytes<T,B>& rhs) {
		return in >> static_cast<T&>(rhs);
	}

	template<class T>
	struct Binary {
		explicit Binary(const T& v): val(v) {}
		friend std::ostream& operator<<(std::ostream& out, const Binary& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				std::ostream::fmtflags oldf = out.flags();
				out << std::hex << std::setfill('0');
				std::for_each(rhs.val.begin(), rhs.val.end(), [&out] (char ch) {
					out << std::setw(2)
						<< (unsigned int)(unsigned char)ch << ' ';
				});
				out.setf(oldf);
			}
			return out;
		}
	private:
		const T& val;
	};

	namespace components {

		template<class Ifa>
		struct ifaddrs_iterator:
			public std::iterator<std::forward_iterator_tag, Ifa>
		{
			typedef Ifa ifaddrs_type;

			explicit constexpr
			ifaddrs_iterator(ifaddrs_type* rhs): _addr(rhs) {}
			constexpr ifaddrs_iterator() = default;
			inline ~ifaddrs_iterator() = default;
			constexpr ifaddrs_iterator(const ifaddrs_iterator&) = default;
			inline ifaddrs_iterator& operator=(const ifaddrs_iterator&) = default;
			constexpr bool operator==(const ifaddrs_iterator& rhs) const { return this->_addr == rhs._addr; }
			constexpr bool operator!=(const ifaddrs_iterator& rhs) const { return !operator==(rhs); }
		
			inline ifaddrs_type& operator*() { return *this->_addr; }
			constexpr const ifaddrs_type& operator*() const { return *this->_addr; }
			inline ifaddrs_type* operator->() { return this->_addr; }
			constexpr const ifaddrs_type* operator->() const { return this->_addr; }
			inline ifaddrs_iterator& operator++() { this->next(); return *this; }
			inline ifaddrs_iterator& operator++(int) {
				ifaddrs_iterator tmp(this); this->next(); return tmp;
			}

		private:
			inline void next() { this->_addr = this->_addr->ifa_next; }

			ifaddrs_type* _addr;
		};

		struct Ifaddrs {

			typedef struct ::ifaddrs ifaddrs_type;
			typedef ifaddrs_type value_type;
			typedef ifaddrs_iterator<ifaddrs_type> iterator;
			typedef std::size_t size_type;

			Ifaddrs() {
				check(::getifaddrs(&this->_addrs),
					__FILE__, __LINE__, __func__);
			}
			~Ifaddrs() { ::freeifaddrs(this->_addrs); }

			iterator begin() { return iterator(this->_addrs); }
			iterator begin() const { return iterator(this->_addrs); }
			static constexpr iterator end() { return iterator(); }

			bool empty() const { return this->_addrs->ifa_next == nullptr; }
			size_type size() const {
				return std::count_if(this->begin(), this->end(),
					[] (const value_type&) { return true; });
			}

		private:
			ifaddrs_type* _addrs = nullptr;
		};

	}

}
