#ifndef SYSX_PACKSTREAM_HH
#define SYSX_PACKSTREAM_HH

#include <istream>
#include <ostream>

#include <sysx/network_format.hh>

namespace sysx {

	template<class Ch, class Tr=std::char_traits<Ch>, class Size=uint32_t>
	struct basic_packstream: public std::basic_iostream<Ch,Tr> {

		typedef std::basic_iostream<Ch,Tr> iostream_type;
		typedef std::basic_streambuf<Ch,Tr> streambuf_type;
		typedef Ch char_type;
		typedef basic_packstream<Ch,Tr,Size> this_type;

		explicit
		basic_packstream(streambuf_type* str):
		iostream_type(str) {}

		basic_packstream(basic_packstream&& rhs):
		iostream_type(rhs.rdbuf()) {}

		basic_packstream(const basic_packstream&) = delete;
		basic_packstream() = delete;

		basic_packstream& operator<<(bool rhs) { return write(rhs ? char(1) : char(0)); }
		basic_packstream& operator<<(char rhs) { return write(rhs); }
		basic_packstream& operator<<(int8_t rhs)  { return write(rhs); }
		basic_packstream& operator<<(int16_t rhs) { return write(rhs); }
		basic_packstream& operator<<(int32_t rhs) { return write(rhs); }
		basic_packstream& operator<<(int64_t rhs) { return write(rhs); }
		basic_packstream& operator<<(uint8_t rhs) { return write(rhs); }
		basic_packstream& operator<<(uint16_t rhs) { return write(rhs); }
		basic_packstream& operator<<(uint32_t rhs) { return write(rhs); }
		basic_packstream& operator<<(uint64_t rhs) { return write(rhs); }
		basic_packstream& operator<<(float rhs) { return write(rhs); }
		basic_packstream& operator<<(double rhs) { return write(rhs); }
		#ifdef __SIZEOF_LONG_DOUBLE__
		basic_packstream& operator<<(long double rhs) { return write(rhs); }
		#endif
		basic_packstream& operator<<(const std::string& rhs) { return write(rhs); }
		template<class T>
		basic_packstream& operator<<(const sysx::Bytes<T>& rhs) {
			return this->write(rhs.begin(), rhs.size());
		}

		basic_packstream& operator>>(bool& rhs) {
			char c = 0; read(c); rhs = c == 1; return *this;
		}
		basic_packstream& operator>>(char& rhs) { return read(rhs); }
		basic_packstream& operator>>(int8_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(int16_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(int32_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(int64_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(uint8_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(uint16_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(uint32_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(uint64_t& rhs) { return read(rhs); }
		basic_packstream& operator>>(float& rhs) { return read(rhs); }
		basic_packstream& operator>>(double& rhs) { return read(rhs); }
		#ifdef __SIZEOF_LONG_DOUBLE__
		basic_packstream& operator>>(long double& rhs) { return read(rhs); }
		#endif
		basic_packstream& operator>>(std::string& rhs) { return read(rhs); }
		template<class T>
		basic_packstream& operator>>(sysx::Bytes<T>& rhs) {
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
		basic_packstream& write(T rhs) {
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

		basic_packstream& write(const std::string& rhs) {
			Size length = static_cast<Size>(rhs.size());
			write(length);
			this->iostream_type::write(rhs.c_str(), length);
			return *this;
		}

		template<class T>
		basic_packstream& read(T& rhs) {
			sysx::Bytes<T> val;
			this->iostream_type::read(val.begin(), val.size());
			val.to_host_format();
			rhs = val;
			return *this;
		}

		basic_packstream& read(std::string& rhs) {
			Size length;
			read(length);
			std::string::value_type* bytes = new std::string::value_type[length];
			this->iostream_type::read(bytes, length);
			rhs.assign(bytes, bytes + length);
			delete[] bytes;
			return *this;
		}

	};

	typedef basic_packstream<char> packstream;

}

#endif // SYSX_PACKSTREAM_HH
