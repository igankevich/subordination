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

	template<class T>
	union Bytes {

		typedef Integer<sizeof(T)> Int;

		Bytes() {}
		Bytes(T v): val(v) {}

		void to_network_format() { Int::to_network_format(i); }
		void to_host_format() { Int::to_host_format(i); }

		operator T& () { return val; }
		operator const T& () const { return val; }
		operator char* () { return bytes; }
		operator const char* () const { return bytes; }

		bool operator==(const Bytes<T>& rhs) const { return i == rhs.i; }
		bool operator!=(const Bytes<T>& rhs) const { return i != rhs.i; }

	private:
		T val;
		typename Int::value i;
		char bytes[sizeof(T)];

		static_assert(
			sizeof(decltype(val)) == sizeof(decltype(i)),
			"Bad size of integral type.");
	};

	template<class T>
	std::ostream& operator<<(std::ostream& out, const Bytes<T>& rhs) {
		return out << static_cast<const T&>(rhs);
	}

	template<class T>
	std::istream& operator>>(std::istream& in, Bytes<T>& rhs) {
		return in >> static_cast<T&>(rhs);
	}

}
