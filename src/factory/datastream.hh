namespace factory {

	struct Foreign_stream {

		typedef char Byte;
		typedef typename Buffer<Byte>::Size Size;

		explicit Foreign_stream(Size buffer_size = DEFAULT_BUFFER_SIZE):
			_buffer(buffer_size) {}

		Foreign_stream& operator<<(char rhs) { return write(rhs); }
#ifdef INT8_MAX
		Foreign_stream& operator<<(int8_t rhs)  { return write(rhs); }
#endif
#ifdef INT16_MAX
		Foreign_stream& operator<<(int16_t rhs) { return write(rhs); }
#endif
#ifdef INT32_MAX
		Foreign_stream& operator<<(int32_t rhs) { return write(rhs); }
#endif
#ifdef INT64_MAX
		Foreign_stream& operator<<(int64_t rhs) { return write(rhs); }
#endif
#ifdef UINT8_MAX
		Foreign_stream& operator<<(uint8_t rhs) { return write(rhs); }
#endif
#ifdef UINT16_MAX
		Foreign_stream& operator<<(uint16_t rhs) { return write(rhs); }
#endif
#ifdef UINT32_MAX
		Foreign_stream& operator<<(uint32_t rhs) { return write(rhs); }
#endif
#ifdef UINT64_MAX
		Foreign_stream& operator<<(uint64_t rhs) { return write(rhs); }
#endif
		Foreign_stream& operator<<(float rhs) { return write(rhs); }
		Foreign_stream& operator<<(double rhs) { return write(rhs); }
		Foreign_stream& operator<<(long double rhs) { return write(rhs); }
		Foreign_stream& operator<<(const std::string& rhs) { return write(rhs); }

		Foreign_stream& operator>>(char& rhs) { return read(rhs); }
		Foreign_stream& operator>>(int8_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(int16_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(int32_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(int64_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(uint8_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(uint16_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(uint32_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(uint64_t& rhs) { return read(rhs); }
		Foreign_stream& operator>>(float& rhs) { return read(rhs); }
		Foreign_stream& operator>>(double& rhs) { return read(rhs); }
		Foreign_stream& operator>>(long double& rhs) { return read(rhs); }
		Foreign_stream& operator>>(std::string& rhs) { return read(rhs); }

		void write(const Byte* bytes, Size size) { _buffer.write(bytes, size); }
		Size read(Byte* bytes, Size size) { return _buffer.read(bytes, size); }

		template<class S> void fill(S& source) { _buffer.fill(source); }
		template<class S> void flush(S& sink) { _buffer.flush(sink); }

		void reset() { _buffer.reset(); }
		void write_size() { _buffer.declared_size(network_format(_buffer.size())); }
		bool full() const { return _buffer.size() == host_format(_buffer.declared_size()); }
		bool empty() const { return _buffer.size() == 0; }

		friend std::ostream& operator<<(std::ostream& out, const Foreign_stream& rhs) {
			return out << rhs._buffer;
		}

	private:

		template<class T>
		static void debug(std::ostream& out, T val) {
			int n = sizeof val;
			unsigned char* p = reinterpret_cast<unsigned char*>(&val);
			for (int i=0; i<n; ++i) {
				out << std::setw(2) << (unsigned int)p[i];
			}
		}

		template<class T>
		Foreign_stream& write(T rhs) {
#ifndef IGNORE_ISO_IEC559
			static_assert(std::is_integral<T>::value
				|| (std::is_floating_point<T>::value && std::numeric_limits<T>::is_iec559), 
				"This system does not support ISO IEC 559"
	            " floating point representation for either float, double or long double"
	            " types, i.e. there is no portable way of"
	            " transmitting floating point numbers over the network"
	            " without precision loss. If all computers in the network do not"
	            " conform to this standard but represent floating point"
	            " numbers exactly in the same way, you can ignore this assertion"
	            " by defining IGNORE_ISO_IEC559.");
#endif
			Bytes<T> tmp = rhs;
			Bytes<T> val;
			val.i = network_format(tmp.i);
//			std::clog << "Converted from " << std::hex << std::setfill('0');
//			debug(std::clog, rhs);
//			std::clog << " to ";
//			debug(std::clog, val.val);
//			std::clog << std::dec << std::endl;
			write(val.bytes, sizeof(rhs));
			return *this;
		}

		Foreign_stream& write(const std::string& rhs) {
			Size length = rhs.size();
//			std::clog << "Writing string of length = " << length << std::endl;
			write(length);
			write(rhs.c_str(), length*sizeof(std::string::value_type));
			return *this;
		}

		template<class T>
		Foreign_stream& read(T& rhs) {
			Bytes<T> val;
			read(val.bytes, sizeof(rhs));
			Bytes<T> tmp;
			tmp.i = host_format(val.i);
			rhs = tmp.val;
//			std::clog << "Converted from " << std::hex << std::setfill('0');
//			debug(std::clog, val);
//			std::clog << " to ";
//			debug(std::clog, rhs);
//			std::clog << std::dec << std::endl;
			return *this;
		}

		Foreign_stream& read(std::string& rhs) {
			Size length;
			read(length);
			std::string::value_type* bytes = new std::string::value_type[length];
//			std::clog << "Reading string of length = " << length << std::endl;
			read(bytes, length);
			rhs.assign(bytes, bytes + length);
			delete[] bytes;
			return *this;
		}

		Buffer<Byte> _buffer;

		static const Size DEFAULT_BUFFER_SIZE = 128;
	};

}
