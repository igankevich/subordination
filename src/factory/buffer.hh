namespace factory {

	template<class T>
	struct Buffer {

		typedef uint32_t Size;
		typedef uint32_t Pos;
		typedef T Value;

		explicit Buffer(Size base_size = CHUNK_SIZE):
			_chunk_size(std::max(base_size, MIN_CHUNK_SIZE)),
			_chunks(),
			_read_pos(0),
			_write_pos(0),
			_chunk_write_pos(0),
			_global_read_pos(0),
			_global_write_pos(0)
		{
			_chunks.push_back(new T[_chunk_size]);
		}

		void reset() {
			_read_pos = 0;
			_write_pos = 0;
			_chunk_write_pos = 0;
//			_global_read_pos = 0;
//			_global_write_pos = 0;
		}

		~Buffer() {
			while (!_chunks.empty()) {
				T* chunk = _chunks.front();
				delete[] chunk;
				_chunks.pop_front();
			}
		}

		Size read(T* buffer, Size size) {
			Size offset = 0;
			while (offset != size && !_chunks.empty()) {
				Size m = (last_chunk() ? _write_pos : _chunk_size);
				Size n = std::min(m - _read_pos, size - offset);
				T* chunk = _chunks.front();
				std::copy(chunk + _read_pos, chunk + _read_pos + n, buffer + offset);
				_read_pos += n;
				_global_read_pos += n;
				offset += n;
				if (_read_pos == _chunk_size) {
					remove_chunk();
					_read_pos = 0;
				}
			}
			return offset;
		}

		void write(const T* buffer, Size size) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
			}
			Size offset = 0;
			while (offset != size) {
				T* chunk = _chunks[_chunk_write_pos];
				Size n = std::min(_chunk_size - _write_pos, size - offset);
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
		ssize_t fill(S source) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
			}
			ssize_t bytes_read;
			T* chunk = _chunks.back();
			while ((bytes_read = source.read(chunk + _write_pos, _chunk_size - _write_pos)) > 0) {
				Logger(Level::COMPONENT) << "Bytes read = " << bytes_read << std::endl;
				_write_pos += bytes_read;
				_global_write_pos += bytes_read;
				if (_write_pos == _chunk_size) {
					add_chunk();
					chunk = _chunks.back();
					_write_pos = 0;
				}
			}
			Logger(Level::COMPONENT) << "Bytes read = " << bytes_read << std::endl;
			return bytes_read;
		}

		template<class S>
		ssize_t flush(S sink) {
			ssize_t n = 0;
			T* chunk = _chunks.front();
			Size m; 
			while (n = 0, m = (last_chunk() ? _write_pos : _chunk_size), !_chunks.empty()
				&& (n = sink.write(chunk + _read_pos, m - _read_pos)) > 0)
			{
				Logger(Level::COMPONENT) << "Bytes written = " << n << std::endl;
				_read_pos += n;
				_global_read_pos += n;
				if (_read_pos == _chunk_size) {
					remove_chunk();
					chunk = _chunks.front();
					_read_pos = 0;
				}
			}
			Logger(Level::COMPONENT) << "Chunks = " << _chunks.size() << std::endl;
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
			out << std::hex << std::setfill('0');
			if (rhs._chunks.size() > 1) {
				std::for_each(rhs._chunks.cbegin(), rhs._chunks.cend()-1, [&rhs, &out] (T* chunk) {
					for (size_t i=0; i<rhs._chunk_size; ++i) {
						out << std::setw(2) << (unsigned int)(unsigned char)chunk[i];
					}
				});
			}
			if (!rhs._chunks.empty()) {
				T* chunk = rhs._chunks.back();
				size_t n = std::max(rhs._write_pos, rhs._read_pos);
				for (size_t i=0; i<n; ++i) {
					out << std::setw(2) << (unsigned int)(unsigned char)chunk[i];
				}
			}
			out << std::dec << std::setfill(' ');
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
		Size _read_pos;
		Size _write_pos;
		Size _chunk_write_pos;

		Size _global_read_pos;
		Size _global_write_pos;

		static const Size CHUNK_SIZE = 128;
		static const Size MIN_CHUNK_SIZE = 1;
	};

}
