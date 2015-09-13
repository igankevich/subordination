#ifndef SYSX_PACKETSTREAM_HH
#define SYSX_PACKETSTREAM_HH

#include <istream>
#include <ostream>

#include <sysx/network_format.hh>

namespace sysx {

	template<class Ch, class Tr=std::char_traits<Ch>, class Size=uint32_t>
	struct basic_packetstream: public std::basic_iostream<Ch,Tr> {

		typedef std::basic_iostream<Ch,Tr> iostream_type;
		typedef std::basic_streambuf<Ch,Tr> streambuf_type;
		typedef Ch char_type;
		typedef basic_packetstream<Ch,Tr,Size> this_type;

		explicit
		basic_packetstream(streambuf_type* str):
		iostream_type(str) {}

		basic_packetstream(basic_packetstream&& rhs):
		iostream_type(rhs.rdbuf()) {}

		basic_packetstream(const basic_packetstream&) = delete;
		basic_packetstream() = delete;

		basic_packetstream& operator<<(bool rhs) { return write(rhs ? char(1) : char(0)); }
		basic_packetstream& operator<<(char rhs) { return write(rhs); }
		basic_packetstream& operator<<(int8_t rhs)  { return write(rhs); }
		basic_packetstream& operator<<(int16_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(int32_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(int64_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(uint8_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(uint16_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(uint32_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(uint64_t rhs) { return write(rhs); }
		basic_packetstream& operator<<(float rhs) { return write(rhs); }
		basic_packetstream& operator<<(double rhs) { return write(rhs); }
		#ifdef __SIZEOF_LONG_DOUBLE__
		basic_packetstream& operator<<(long double rhs) { return write(rhs); }
		#endif
		basic_packetstream& operator<<(const std::string& rhs) { return write(rhs); }
		template<class T>
		basic_packetstream& operator<<(const sysx::Bytes<T>& rhs) {
			return this->write(rhs.begin(), rhs.size());
		}

		basic_packetstream& operator>>(bool& rhs) {
			char c = 0; read(c); rhs = c == 1; return *this;
		}
		basic_packetstream& operator>>(char& rhs) { return read(rhs); }
		basic_packetstream& operator>>(int8_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(int16_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(int32_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(int64_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(uint8_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(uint16_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(uint32_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(uint64_t& rhs) { return read(rhs); }
		basic_packetstream& operator>>(float& rhs) { return read(rhs); }
		basic_packetstream& operator>>(double& rhs) { return read(rhs); }
		#ifdef __SIZEOF_LONG_DOUBLE__
		basic_packetstream& operator>>(long double& rhs) { return read(rhs); }
		#endif
		basic_packetstream& operator>>(std::string& rhs) { return read(rhs); }
		template<class T>
		basic_packetstream& operator>>(sysx::Bytes<T>& rhs) {
			return this->read(rhs.begin(), rhs.size());
		}

		this_type& write(const Ch* buf, std::streamsize n) {
			this->iostream_type::write(buf, n);
			return *this;
		}

		this_type& read(Ch* buf, std::streamsize n) {
			this->iostream_type::read(buf, n);
			return *this;
		}

	private:

		template<class T>
		basic_packetstream& write(T rhs) {
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
			sysx::Bytes<T> val = rhs;
			val.to_network_format();
			this->operator<<(val);
			return *this;
		}

		basic_packetstream& write(const std::string& rhs) {
			Size length = static_cast<Size>(rhs.size());
			write(length);
			this->iostream_type::write(rhs.c_str(), length);
			return *this;
		}

		template<class T>
		basic_packetstream& read(T& rhs) {
			sysx::Bytes<T> val;
			this->iostream_type::read(val.begin(), val.size());
			val.to_host_format();
			rhs = val;
			return *this;
		}

		basic_packetstream& read(std::string& rhs) {
			Size length;
			read(length);
			std::string::value_type* bytes = new std::string::value_type[length];
			this->iostream_type::read(bytes, length);
			rhs.assign(bytes, bytes + length);
			delete[] bytes;
			return *this;
		}

	};

	typedef basic_packetstream<char> packetstream;

}

#endif // SYSX_PACKETSTREAM_HH
