namespace factory {

	template<class T>
	union Bytes {

		Bytes() {}
		Bytes(T v): val(v) {}

		T val;
		T i;
		char bytes[sizeof(T)];
	};

	template<>
	union Bytes<float> {
		typedef float T;

		Bytes() {}
		Bytes(T v): val(v) {}

		T val;
		uint32_t i;
		char bytes[sizeof(T)];
	};

	template<>
	union Bytes<double> {
		typedef double T;

		Bytes() {}
		Bytes(T v): val(v) {}

		T val;
		uint64_t i;
		char bytes[sizeof(T)];
	};

	
	namespace components {

		template<class T>
		struct base_format {
			base_format(T value): _value(value) {}
			operator T() const { return _value; }
		protected:
			T _value;
		};

		template<class T, size_t bytes>
		struct network_format: public base_format<T> {
			network_format(T value): base_format<T>(value) {}
			operator T() const {
#if __BYTE_ORDER == __LITTLE_ENDIAN
				const char* raw = reinterpret_cast<const char*>(&(base_format<T>::_value));
				T newval;
				std::reverse_copy(raw, raw + bytes, (char*)&newval);
				return newval;
#else
				return base_format<T>::_value;
#endif
			}
		};

		template<class T>
		struct network_format<T, 1>: public base_format<T> {
			network_format(T value): base_format<T>(value) {}
			operator T() const { return base_format<T>::_value; }
		};

		template<class T>
		struct network_format<T, 2>: public base_format<T> {
			network_format(T value): base_format<T>(value) {}
			operator T() const { return htobe16(base_format<T>::_value); }
		};

		template<class T>
		struct network_format<T, 4>: public base_format<T> {
			network_format(T value): base_format<T>(value) {}
			operator T() const { return htobe32(base_format<T>::_value); }
		};

		template<class T>
		struct network_format<T, 8>: public base_format<T> {
			network_format(T value): base_format<T>(value) {}
			operator T() const { return htobe64(base_format<T>::_value); }
		};

		// -----------
		// Host format
		// -----------

		template<class T, size_t bytes>
		struct host_format: public base_format<T> {
			host_format(T value): base_format<T>(value) {}
			operator T() const {
#if __BYTE_ORDER == __LITTLE_ENDIAN
				const char* raw = reinterpret_cast<const char*>(&(base_format<T>::_value));
				T newval;
				std::reverse_copy(raw, raw + bytes, (char*)&newval);
				return newval;
#else
				return base_format<T>::_value;
#endif
			}
		};

		template<class T>
		struct host_format<T, 1>: public base_format<T> {
			host_format(T value): base_format<T>(value) {}
			operator T() const { return base_format<T>::_value; }
		};

		template<class T>
		struct host_format<T, 2>: public base_format<T> {
			host_format(T value): base_format<T>(value) {}
			operator T() const { return be16toh(base_format<T>::_value); }
		};

		template<class T>
		struct host_format<T, 4>: public base_format<T> {
			host_format(T value): base_format<T>(value) {}
			operator T() const { return be32toh(base_format<T>::_value); }
		};

		template<class T>
		struct host_format<T, 8>: public base_format<T> {
			host_format(T value): base_format<T>(value) {}
			operator T() const { return be64toh(base_format<T>::_value); }
		};

	}

	template<class T>
	T host_format(T value) {
		return components::host_format<T, sizeof(T)>(value);
	}

	template<class T>
	T network_format(T value) {
		return components::network_format<T, sizeof(T)>(value);
	}

}
