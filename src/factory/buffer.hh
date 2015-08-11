namespace factory {

	namespace components {

		template<class T>
		struct LBuffer {
			
			typedef T value_type;
			typedef size_t Size;

			explicit LBuffer(size_t base_size): _data(base_size) {}

			void push_back(const T byte) {
				if (write_pos + 1 == buffer_size()) { double_size(); }
				_data[write_pos] = byte;
				++write_pos;
			}

			const T& front() const { return _data[read_pos]; }
			T& front() { return _data[read_pos]; }

			void pop_front() {
				if (!empty()) {
					advance_read_pos(1);
				}
			}

			size_t write(const T* buf, size_t sz) {
				while (write_pos + sz >= buffer_size()) { double_size(); }
				std::copy(buf, buf + sz, &_data[write_pos]);
				write_pos += sz;
				return sz;
			}

			size_t read(T* buf, size_t sz) {
				size_t min_sz = std::min(sz, size()); 
				std::copy(&_data[read_pos], &_data[read_pos + min_sz], buf);
				advance_read_pos(min_sz);
				return min_sz;
			}

			size_t skip(size_t n) {
				size_t min_n = std::min(n, size()); 
				advance_read_pos(min_n);
				return min_n;
			}

			size_t readsome(T* buf, size_t sz) {
				size_t min_sz = std::min(sz, size()); 
				std::copy(&_data[read_pos], &_data[read_pos + min_sz], buf);
				return min_sz;
			}

			size_t ignore(size_t nbytes) {
				size_t min_sz = std::min(nbytes, size()); 
				advance_read_pos(min_sz);
				return min_sz;
			}

			void reset() {
				read_pos = 0;
				write_pos = 0;
			}

			bool empty() const { return size() == 0; }
			size_t size() const { return write_pos - read_pos; }
			size_t buffer_size() const { return _data.size(); }
			size_t write_size() const { return buffer_size() - write_pos; }

			T* write_begin() { return &_data[write_pos]; }
			T* write_end() { return &_data[buffer_size()]; }

			T* read_begin() { return &_data[read_pos]; }
			T* read_end() { return &_data[write_pos]; }

			T* begin() { return read_begin(); }
			T* end() { return read_end(); }

			const T* begin() const { return &_data[read_pos]; }
			const T* end() const { return &_data[write_pos]; }

			template<class S>
			size_t flush(S sink) {
				size_t bytes_written;
				size_t total = read_pos;
				while ((bytes_written = sink.write(read_begin(), write_pos-read_pos)) > 0) {
					read_pos += bytes_written;
					Logger<Level::COMPONENT>() << "bytes written = " << bytes_written << std::endl;
				}
				total = read_pos - total;
				Logger<Level::COMPONENT>()
					<< "readpos = " << read_pos
					<< ", writepos = " << write_pos
					<< std::endl;
				if (read_pos == write_pos) {
					reset();
				}
				return total;
			}

			template<class S>
			size_t fill(S source) {
				size_t bytes_read;
				size_t old_pos = write_pos;
				while ((bytes_read = source.read(write_begin(), write_size())) > 0) {
					write_pos += bytes_read;
					if (write_pos == buffer_size()) {
						double_size();
					}
				}
				return write_pos - old_pos;
			}

			friend std::ostream& operator<<(std::ostream& out, const LBuffer<T>& rhs) {
				out << std::hex << std::setfill('0');
				size_t max_elems = out.width() / 2;
				auto last = std::min(rhs.begin() + max_elems, rhs.end());
				std::for_each(rhs.begin(), last, [&out] (T x) {
					out << std::setw(2) << to_binary(x);
				});
				if (last != rhs.end()) {
					out << "...(truncated)";
				}
				return out;
			}

		private:
			static unsigned int to_binary(T x) {
				return static_cast<unsigned int>(static_cast<unsigned char>(x));
			}

			void advance_read_pos(size_t amount) {
				read_pos += amount;
				if (read_pos == write_pos) {
					reset();
				}
			}

			void double_size() { _data.resize(buffer_size()*2u); }

			std::vector<T> _data;
			size_t write_pos = 0;
			size_t read_pos = 0;
		};

		template<class Container>
		class front_pop_iterator:
		    public std::iterator<std::input_iterator_tag, typename Container::value_type>
		{
		public:
			typedef Container container_type;
			typedef typename Container::value_type value_type;
			typedef typename Container::value_type iterator;

			explicit front_pop_iterator(Container& x):
				container(&x), ptr(x.begin()) {}
			front_pop_iterator(Container& x, iterator it):
				container(&x), ptr(it) {}
			front_pop_iterator(front_pop_iterator& rhs):
				container(rhs.container), ptr(rhs.ptr) {}

			std::ptrdiff_t operator-(const front_pop_iterator<Container>& rhs) {
				return ptr - rhs.ptr;
			}
			front_pop_iterator<Container> operator+(std::ptrdiff_t rhs) const {
				return front_pop_iterator<Container>(container,
					std::advance(container.begin(), rhs));
			}
			bool operator==(const front_pop_iterator<Container>& rhs) const {
				return ptr == rhs.ptr;
			}
			bool operator!=(const front_pop_iterator<Container>& rhs) const {
				return ptr != rhs.ptr;
			}

			const value_type& operator*() const { return *ptr; }
			const value_type* operator->() const { return ptr; }
			front_pop_iterator<Container>& operator++ () {
				ptr = container.front();
				container.pop_front();
				return *this;
			}

			front_pop_iterator<Container> operator++ (int) {
				front_pop_iterator<Container> tmp = *this;
				++*this;
				return tmp;
			}

		protected:
			Container& container;
			value_type* ptr;
		};

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
			Packing_stream& operator<<(const Bytes<T>& rhs) {
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
			Packing_stream& operator>>(Bytes<T>& rhs) {
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

//			template<class T>
//			static void debug(std::ostream& out, T val) {
//				int n = sizeof val;
//				unsigned char* p = reinterpret_cast<unsigned char*>(&val);
//				for (int i=0; i<n; ++i) {
//					out << std::setw(2) << (unsigned int)p[i];
//				}
//			}

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
				Bytes<T> val = rhs;
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
				this->iostream_type::write(reinterpret_cast<const char_type*>(rhs.c_str()), length);
				return *this;
			}

			template<class T>
			Packing_stream& read(T& rhs) {
				Bytes<T> val;
				this->iostream_type::read(static_cast<Ch*>(val), sizeof(rhs));
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
				this->iostream_type::read(reinterpret_cast<char_type*>(bytes), length);
				rhs.assign(bytes, bytes + length);
				delete[] bytes;
				return *this;
			}

		};

	}

	typedef components::Packing_stream<char> packstream;

}
