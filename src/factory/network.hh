namespace factory {

	template<size_t bytes>
	struct Integer {
		typedef uint8_t value[bytes];
#if __BYTE_ORDER == __LITTLE_ENDIAN
		static void to_network_format(value& val) { std::reverse(val, val + bytes); }
		static void to_host_format(value& val) { std::reverse(val, val + bytes); }
#else
		static void to_network_format(value&) {}
		static void to_host_format(value&) {}
#endif
	};

	template<> struct Integer<1> {
		typedef uint8_t value;
		static void to_network_format(value&) {}
		static void to_host_format(value&) {}
	};

	template<> struct Integer<2> {
		typedef uint16_t value;
		static void to_network_format(value& val) { val = htobe16(val); }
		static void to_host_format(value& val) { val = be16toh(val); }
	};

	template<> struct Integer<4> {
		typedef uint32_t value;
		static void to_network_format(value& val) { val = htobe32(val); }
		static void to_host_format(value& val) { val = be32toh(val); }
	};

	template<> struct Integer<8> {
		typedef uint64_t value;
		static void to_network_format(value& val) { val = htobe64(val); }
		static void to_host_format(value& val) { val = be64toh(val); }
	};

	template<class T, class Byte=char>
	union Bytes {

		typedef Integer<sizeof(T)> Int;

		Bytes() {}
		Bytes(T v): val(v) {}
		template<class It>
		Bytes(It first, It last) { std::copy(first, last, bytes); }

		Bytes<T,Byte>& to_network_format() { Int::to_network_format(i); return *this; }
		Bytes<T,Byte>& to_host_format() { Int::to_host_format(i); return *this; }

		operator T& () { return val; }
		operator const T& () const { return val; }
		operator Byte* () { return bytes; }
		operator const Byte* () const { return bytes; }

		Byte operator[](size_t idx) const { return bytes[idx]; }

		bool operator==(const Bytes<T>& rhs) const { return i == rhs.i; }
		bool operator!=(const Bytes<T>& rhs) const { return i != rhs.i; }

		Byte* begin() { return bytes; }
		Byte* end() { return bytes + sizeof(T); }

		const Byte* begin() const { return bytes; }
		const Byte* end() const { return bytes + sizeof(T); }

		const T& value() const { return val; }
		T& value() { return val; }

	private:
		T val;
		typename Int::value i;
		Byte bytes[sizeof(T)];

		static_assert(sizeof(decltype(val)) == sizeof(decltype(i)),
			"Bad size of integral type.");

//		static_assert(std::is_arithmetic<T>::value, 
//			"Bad type for byte representation.");
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
