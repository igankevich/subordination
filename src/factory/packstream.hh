#include "bits/byte_swap.hh"

namespace factory {

	namespace components {

		template<class Ch, class Tr=std::char_traits<Ch>, class Size=uint32_t>
		struct Packing_stream: public std::basic_iostream<Ch,Tr> {

			typedef std::basic_iostream<Ch,Tr> iostream_type;
			typedef std::basic_streambuf<Ch,Tr> streambuf_type;
			typedef Ch char_type;
			typedef Packing_stream<Ch,Tr,Size> this_type;

			explicit Packing_stream(streambuf_type* str): iostream_type(str) {
//				this->init(str);
			}
			Packing_stream(Packing_stream&& rhs): iostream_type(rhs.rdbuf()) {}
			Packing_stream(const Packing_stream&) = delete;
			Packing_stream() = delete;

			Packing_stream& operator<<(bool rhs) { return write(rhs ? char(1) : char(0)); }
			Packing_stream& operator<<(char rhs) { return write(rhs); }
			Packing_stream& operator<<(int8_t rhs)  { return write(rhs); }
			Packing_stream& operator<<(int16_t rhs) { return write(rhs); }
			Packing_stream& operator<<(int32_t rhs) { return write(rhs); }
			Packing_stream& operator<<(int64_t rhs) { return write(rhs); }
			Packing_stream& operator<<(uint8_t rhs) { return write(rhs); }
			Packing_stream& operator<<(uint16_t rhs) { return write(rhs); }
			Packing_stream& operator<<(uint32_t rhs) { return write(rhs); }
			Packing_stream& operator<<(uint64_t rhs) { return write(rhs); }
			Packing_stream& operator<<(float rhs) { return write(rhs); }
			Packing_stream& operator<<(double rhs) { return write(rhs); }
//			Packing_stream& operator<<(long double rhs) { return write(rhs); }
			Packing_stream& operator<<(const std::string& rhs) { return write(rhs); }
			template<class T>
			Packing_stream& operator<<(const bits::Bytes<T>& rhs) {
				return this->write(rhs.begin(), rhs.size());
			}

			Packing_stream& operator>>(bool& rhs) {
				char c = 0; read(c); rhs = c == 1; return *this;
			}
			Packing_stream& operator>>(char& rhs) { return read(rhs); }
			Packing_stream& operator>>(int8_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(int16_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(int32_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(int64_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(uint8_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(uint16_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(uint32_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(uint64_t& rhs) { return read(rhs); }
			Packing_stream& operator>>(float& rhs) { return read(rhs); }
			Packing_stream& operator>>(double& rhs) { return read(rhs); }
//			Packing_stream& operator>>(long double& rhs) { return read(rhs); }
			Packing_stream& operator>>(std::string& rhs) { return read(rhs); }
			template<class T>
			Packing_stream& operator>>(bits::Bytes<T>& rhs) {
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
			Packing_stream& write(T rhs) {
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
				bits::Bytes<T> val = rhs;
				val.to_network_format();
//				Logger<Level::IO>() << "Converted from " << std::hex << std::setfill('0');
//				debug(Logger<Level::IO>(), rhs);
//				Logger<Level::IO>() << " to ";
//				debug(Logger<Level::IO>(), val.val);
//				Logger<Level::IO>() << std::dec << std::endl;
//				this->iostream_type::write(static_cast<const Ch*>(val), sizeof(rhs));
				this->operator<<(val);
				return *this;
			}

			Packing_stream& write(const std::string& rhs) {
				Size length = static_cast<Size>(rhs.size());
//				Logger<Level::IO>() << "Writing string of length = " << length << std::endl;
				write(length);
				this->iostream_type::write(rhs.c_str(), length);
				return *this;
			}

			template<class T>
			Packing_stream& read(T& rhs) {
				bits::Bytes<T> val;
				this->iostream_type::read(val.begin(), val.size());
				val.to_host_format();
				rhs = val;
//				Logger<Level::IO>() << "Converted from " << std::hex << std::setfill('0');
//				debug(Logger<Level::IO>(), val);
//				Logger<Level::IO>() << " to ";
//				debug(Logger<Level::IO>(), rhs);
//				Logger<Level::IO>() << std::dec << std::endl;
				return *this;
			}

			Packing_stream& read(std::string& rhs) {
				Size length;
				read(length);
				std::string::value_type* bytes = new std::string::value_type[length];
//				Logger<Level::IO>() << "Reading string of length = " << length << std::endl;
				this->iostream_type::read(bytes, length);
				rhs.assign(bytes, bytes + length);
				delete[] bytes;
				return *this;
			}

		};

		typedef Packing_stream<char> packstream;
	}

	using components::packstream;

}
