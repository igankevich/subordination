namespace factory {

	template<class T>
	struct Buffer {

		typedef uint32_t Size;
		typedef T Value;

		static const Size RESERVED_SPACE = sizeof(Size);

		explicit Buffer(Size base_size):
			_chunk_size(std::max(base_size, RESERVED_SPACE)),
			_chunks(),
			_read_pos(RESERVED_SPACE),
			_write_pos(RESERVED_SPACE),
			_read_pos_flush(0),
			_write_pos_fill(0)
		{
//			declared_size(0);
		}

		void reset() {
			_read_pos = RESERVED_SPACE;
			_write_pos = RESERVED_SPACE;
			_read_pos_flush = 0;
			_write_pos_fill = 0;
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
//			factory_log(Level::COMPONENT) << "Chunks = " << _chunks.size() << std::endl;
			while (offset != size && !_chunks.empty()) {
				Size n = std::min(_chunk_size - _read_pos, size - offset);
				T* chunk = _chunks.front();
				std::copy(chunk + _read_pos, chunk + _read_pos + n, buffer + offset);
				_read_pos += n;
				offset += n;
				Size s = (last_chunk() ? _write_pos : _chunk_size);
				if (_read_pos == s) {
					delete[] chunk;
					_chunks.pop_front();
					_read_pos = 0;
				}
			}
			// remove empty chunk
			if (_write_pos == 0) {
				if (last_chunk()) {
					delete[] _chunks.front();
					_chunks.pop_front();
				}
			}
			if (_chunks.size() == 0) {
				reset();
			}
			return offset;
		}

		void write(const T* buffer, Size size) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
				declared_size(0);
			}
			Size offset = 0;
			while (offset != size) {
				Size n = std::min(_chunk_size - _write_pos, size - offset);
				T* chunk = _chunks.back();
				std::copy(buffer + offset, buffer + offset + n, chunk + _write_pos);
				_write_pos += n;
				offset += n;
				if (_write_pos == _chunk_size) {
					_chunks.push_back(new T[_chunk_size]);
					_write_pos = 0;
				}
			}
		}

		template<class S>
		ssize_t fill(S& source) {
			if (_chunks.empty()) {
				_chunks.push_back(new T[_chunk_size]);
				declared_size(0);
			}
			ssize_t bytes_read;
			T* chunk = _chunks.back();
			while ((bytes_read = source.read(chunk + _write_pos_fill, _chunk_size - _write_pos_fill)) > 0) {
				factory_log(Level::COMPONENT) << "Bytes read = " << bytes_read << std::endl;
				_write_pos_fill += bytes_read;
				if (_write_pos_fill == _chunk_size) {
					chunk = new T[_chunk_size];
					_chunks.push_back(chunk);
					_write_pos_fill = 0;
				}
			}
			_write_pos = _write_pos_fill;
			factory_log(Level::COMPONENT) << "Bytes read = " << bytes_read << std::endl;
			factory_log(Level::COMPONENT) << "Declared size = " << host_format(declared_size()) << std::endl;
			return bytes_read;
		}

		template<class S>
		ssize_t flush(S& sink) {
			ssize_t bytes_written = 0;
			T* chunk = _chunks.front();
			Size decl_sz = host_format(declared_size());
			Size num_bytes; 
			while (bytes_written = 0, num_bytes = (last_chunk() ? _write_pos : _chunk_size), !_chunks.empty()
				&& (bytes_written = sink.write(chunk + _read_pos_flush, num_bytes - _read_pos_flush)) > 0)
			{
				factory_log(Level::COMPONENT) << "Bytes written = " << bytes_written << std::endl;
				_read_pos_flush += bytes_written;
				if (_read_pos_flush == num_bytes) {
					delete[] chunk;
					_chunks.pop_front();
					chunk = _chunks.front();
					_read_pos_flush = 0;
				}
			}
			_read_pos = _read_pos_flush;
			// remove empty chunk
			if (_write_pos == 0 && last_chunk()) {
				delete[] _chunks.front();
				_chunks.pop_front();
			}
			if (_chunks.size() == 0) {
				reset();
			}
			factory_log(Level::COMPONENT) << "Bytes written = " << bytes_written << std::endl;
			factory_log(Level::COMPONENT) << "Declared size = " << decl_sz << std::endl;
			factory_log(Level::COMPONENT) << "Chunks = " << _chunks.size() << std::endl;
			return bytes_written;
		}

//		bool empty() const { return _chunks.empty(); }
		bool empty() const { return size() == 0; }
		Size size() const {
			return _write_pos
				+ (_chunks.size() - std::min(size_t(1), _chunks.size())) * _chunk_size
				- RESERVED_SPACE;
		}
//		Size write_pos() const { return _write_pos; }
//		Size read_pos() const { return _read_pos; }

		void declared_size(Size size) {
			T* chunk = _chunks.front();
			T* st = reinterpret_cast<T*>(&size); 
			std::copy(st, st + sizeof size, chunk);
		}

		Size declared_size() const {
			Size size = 0;
			if (!_chunks.empty()) {
				T* chunk = _chunks.front();
				T* st = reinterpret_cast<T*>(&size); 
				std::copy(chunk, chunk + sizeof size, st);
			}
			return size;
		}

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

		bool last_chunk() const { return _chunks.size() == 1; }

		Size _chunk_size;
		std::deque<T*> _chunks;
		Size _read_pos;
		Size _write_pos;
		Size _read_pos_flush;
		Size _write_pos_fill;
	};

}
