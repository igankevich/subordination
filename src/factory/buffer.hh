namespace factory {

	namespace constants {
		static const uint32_t CHUNK_SIZE = 128;
		static const uint32_t MIN_CHUNK_SIZE = 1;
	}

	template<class T>
	struct Buffer {

		typedef uint32_t Size;
		typedef uint32_t Pos;
		typedef uint32_t Nbytes;
		typedef T Value;

		explicit Buffer(Size base_size = constants::CHUNK_SIZE):
			_chunk_size(std::max(base_size, constants::MIN_CHUNK_SIZE)),
			_chunks(),
			_read_pos(0),
			_write_pos(0),
			_chunk_write_pos(0),
			_global_read_pos(0),
			_global_write_pos(0)
		{
			_chunks.push_back(new T[_chunk_size]);
		}

		Buffer(Buffer&& rhs):
			_chunk_size(rhs._chunk_size),
			_chunks(std::move(rhs._chunks)),
			_read_pos(rhs._read_pos),
			_write_pos(rhs._write_pos),
			_chunk_write_pos(rhs._chunk_write_pos),
			_global_read_pos(rhs._global_read_pos),
			_global_write_pos(rhs._global_write_pos) {}

		void reset() {
			_read_pos = 0;
			_write_pos = 0;
			_chunk_write_pos = 0;
//			_global_read_pos = 0;
//			_global_write_pos = 0;
		}

		virtual ~Buffer() {
			while (!_chunks.empty()) {
				T* chunk = _chunks.front();
				delete[] chunk;
				_chunks.pop_front();
			}
		}

		Size read(T* buffer, Size sz) {
			Size offset = 0;
			while (offset != sz && !_chunks.empty()) {
				Size m = (last_chunk() ? _write_pos : _chunk_size);
				Size n = std::min(m - _read_pos, sz - offset);
				T* chunk = _chunks.front();
				std::copy(chunk + _read_pos, chunk + _read_pos + n, buffer + offset);
				_read_pos += n;
				_global_read_pos += n;
				offset += n;
				if (_read_pos == _write_pos && last_chunk()) {
					_read_pos = 0;
					_write_pos = 0;
				}
				if (_read_pos == _chunk_size) {
					remove_chunk();
					_read_pos = 0;
				}
			}
			return offset;
		}

		void write(const T* buffer, Size sz) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
			}
			Size offset = 0;
			while (offset != sz) {
				T* chunk = _chunks[_chunk_write_pos];
				Size n = std::min(_chunk_size - _write_pos, sz - offset);
				std::copy(buffer + offset, buffer + offset + n, chunk + _write_pos);
				_write_pos += n;
				_global_write_pos += n;
				offset += n;
				if (_write_pos == _chunk_size) {
					add_chunk();
					_write_pos = 0;
				}
			}
		}

		template<class S>
		Nbytes fill(S source) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
			}
			Nbytes bytes_read;
			T* chunk = _chunks.back();
			while ((bytes_read = source.read(chunk + _write_pos, _chunk_size - _write_pos)) > 0) {
//				Logger<Level::COMPONENT>() << "Bytes read = " << bytes_read << std::endl;
				_write_pos += bytes_read;
				_global_write_pos += bytes_read;
				if (_write_pos == _chunk_size) {
					add_chunk();
					chunk = _chunks.back();
					_write_pos = 0;
				}
			}
//			Logger<Level::COMPONENT>() << "Bytes read = " << bytes_read << std::endl;
			return bytes_read;
		}

		template<class S>
		Nbytes flush(S sink) {
			Nbytes n = 0;
			T* chunk = _chunks.front();
			Size m; 
			while (n = 0, m = (last_chunk() ? _write_pos : _chunk_size), !_chunks.empty()
				&& (n = sink.write(chunk + _read_pos, m - _read_pos)) > 0)
			{
//				Logger<Level::COMPONENT>() << "Bytes written = " << n << std::endl;
				_read_pos += n;
				_global_read_pos += n;
				if (_read_pos == _chunk_size) {
					remove_chunk();
					chunk = _chunks.front();
					_read_pos = 0;
				}
			}
//			if (n == -1) {
//				if (errno == EPIPE) {
//					Logger<Level::COMPONENT>() << "EPIPE fd=" << (int)sink << std::endl;
//				}
//			}
//			Logger<Level::COMPONENT>() << "Chunks = " << _chunks.size() << std::endl;
			return n;
		}

		bool empty() const { return size() == 0; }

//		Size size() const {
//			return _write_pos
//				+ (_chunks.size() - std::min(size_t(1), _chunks.size())) * _chunk_size
//				- _read_pos;
//		}

		Size size() const { return _global_write_pos - _global_read_pos; }

		std::tuple<Size,Size,Size> write_pos() const {
			return std::make_tuple(_chunk_write_pos, _write_pos, _global_write_pos);
		}

		void write_pos(std::tuple<Size,Size,Size> rhs) {
			std::tie(_chunk_write_pos, _write_pos, _global_write_pos) = rhs;
		}

		Size global_read_pos() const { return _global_read_pos; }
		Size global_write_pos() const { return _global_write_pos; }

		friend std::ostream& operator<<(std::ostream& out, const Buffer& rhs) {
			std::ios_base::fmtflags oldf = out.flags();
			out << std::hex << std::setfill('0');
			switch (rhs._chunks.size()) {
				case 0: break;
				case 1: {
					T* chunk = rhs._chunks.back();
					size_t st = rhs._read_pos;
					size_t en = rhs._write_pos;
					for (size_t i=st; i<en; ++i) {
						out << std::setw(2)
							<< static_cast<unsigned int>(static_cast<unsigned char>(chunk[i]));
					}
				} break;
				default: {
					T* chunk = rhs._chunks.front();
					size_t n = rhs._read_pos;
					for (size_t i=n; i<rhs._chunk_size; ++i) {
						out << std::setw(2)
							<< static_cast<unsigned int>(static_cast<unsigned char>(chunk[i]));
					}
					out << ' ';
					std::for_each(rhs._chunks.cbegin()+1, rhs._chunks.cend()-1, [&rhs, &out] (T* chunk) {
						for (size_t i=0; i<rhs._chunk_size; ++i) {
							out << std::setw(2)
								<< static_cast<unsigned int>(static_cast<unsigned char>(chunk[i]));
							out << ' ';
						}
					});
					chunk = rhs._chunks.back();
					n = rhs._write_pos;
					for (size_t i=0; i<n; ++i) {
						out << std::setw(2)
							<< static_cast<unsigned int>(static_cast<unsigned char>(chunk[i]));
					}
				}
			}
			out.setf(oldf);
			return out;
		}
	
	private:

		void add_chunk() {
			if (++_chunk_write_pos == _chunks.size()) {
				_chunks.push_back(new T[_chunk_size]);
			}
		}

		void remove_chunk() {
			delete[] _chunks.front();
			_chunks.pop_front();
			_chunk_write_pos--;
		}

		bool last_chunk() const { return _chunks.size() == 1; }

		Size _chunk_size;
		std::deque<T*> _chunks;
		Pos _read_pos;
		Pos _write_pos;
		Pos _chunk_write_pos;

		Pos _global_read_pos;
		Pos _global_write_pos;
	};

}
#define FACTORY_FOREIGN_STREAM
namespace factory {

	typedef char Byte;

	struct Foreign_stream: public Buffer<Byte> {

		using Buffer<Byte>::read;
		using Buffer<Byte>::write;
		using Buffer<Byte>::Size;

		explicit Foreign_stream(Size buffer_size = DEFAULT_BUFFER_SIZE):
			Buffer<Byte>(buffer_size) {}

		Foreign_stream& operator<<(bool rhs) { return write(rhs ? char(1) : char(0)); }
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
//		Foreign_stream& operator<<(long double rhs) { return write(rhs); }
		Foreign_stream& operator<<(const std::string& rhs) { return write(rhs); }

		Foreign_stream& operator>>(bool& rhs) {
			char c = 0; read(c); rhs = c == 1; return *this;
		}
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
//		Foreign_stream& operator>>(long double& rhs) { return read(rhs); }
		Foreign_stream& operator>>(std::string& rhs) { return read(rhs); }

	private:

//		template<class T>
//		static void debug(std::ostream& out, T val) {
//			int n = sizeof val;
//			unsigned char* p = reinterpret_cast<unsigned char*>(&val);
//			for (int i=0; i<n; ++i) {
//				out << std::setw(2) << (unsigned int)p[i];
//			}
//		}

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
			Bytes<T> val = rhs;
			val.to_network_format();
//			std::clog << "Converted from " << std::hex << std::setfill('0');
//			debug(std::clog, rhs);
//			std::clog << " to ";
//			debug(std::clog, val.val);
//			std::clog << std::dec << std::endl;
			Buffer<Byte>::write(val, sizeof(rhs));
			return *this;
		}

		Foreign_stream& write(const std::string& rhs) {
			Size length = static_cast<Size>(rhs.size());
//			std::clog << "Writing string of length = " << length << std::endl;
			write(length);
			write(reinterpret_cast<const Byte*>(rhs.c_str()), length);
			return *this;
		}

		template<class T>
		Foreign_stream& read(T& rhs) {
			Bytes<T> val;
			read(val, sizeof(rhs));
			val.to_host_format();
			rhs = val;
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
			read(reinterpret_cast<Byte*>(bytes), length);
			rhs.assign(bytes, bytes + length);
			delete[] bytes;
			return *this;
		}

//		Buffer<Byte> _buffer;

		static const Size DEFAULT_BUFFER_SIZE = (1 << 20) - 0;
	};

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

	
//	struct Trim {
//		constexpr explicit Trim() {}
//		constexpr explicit Trim(int a): amount(a) {}
//		friend std::ostream& operator<<(std::ostream& out, Trim rhs) {
//			const static int index = out.xalloc();
//			out.iword(index) = rhs.amount;
//			return out;		
//		}
//	private:
//		int amount = 80;
//	};

}
