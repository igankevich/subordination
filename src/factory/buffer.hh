namespace factory {

	template<class T>
	struct Buffer {

		typedef uint32_t Size;
		typedef T Value;

		explicit Buffer(Size base_size):
			_chunk_size(std::max(base_size, RESERVED_SPACE)),
			_contents(),
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

		Buffer() {
			while (!_contents.empty()) {
				T* chunk = _contents.front();
				delete[] chunk;
				_contents.pop_front();
			}
		}

		Size read(T* buffer, Size size) {
			Size offset = 0;
			while (offset != size && !_contents.empty()) {
				Size n = std::min(_chunk_size - _read_pos, size - offset);
				T* chunk = _contents.front();
				std::copy(chunk + _read_pos, chunk + _read_pos + n, buffer + offset);
				_read_pos += n;
				offset += n;
				if (_read_pos == _chunk_size) {
					delete[] chunk;
					_contents.pop_front();
					_read_pos = 0;
				}
			}
			return offset;
		}

		void write(const T* buffer, Size size) {
			if (_contents.empty()) {
				_contents.push_back(new T[_chunk_size]);
				declared_size(0);
			}
			Size offset = 0;
			while (offset != size) {
				Size n = std::min(_chunk_size - _write_pos, size - offset);
				T* chunk = _contents.back();
				std::copy(buffer + offset, buffer + offset + n, chunk + _write_pos);
				_write_pos += n;
				offset += n;
				if (_write_pos == _chunk_size) {
					_contents.push_back(new T[_chunk_size]);
					_write_pos = 0;
				}
			}
		}

		template<class S>
		ssize_t fill(S source) {
			if (_contents.empty()) {
				_contents.push_back(new T[_chunk_size]);
				declared_size(0);
			}
			ssize_t bytes_read;
			T* chunk = _contents.back();
			while ((bytes_read = source.read(chunk + _write_pos_fill, _chunk_size - _write_pos_fill)) > 0) {
				std::clog << "Bytes read = " << bytes_read << std::endl;
				_write_pos_fill += bytes_read;
				if (_write_pos_fill == _chunk_size) {
					chunk = new T[_chunk_size];
					_contents.push_back(chunk);
					_write_pos_fill = 0;
				}
			}
			_write_pos = _write_pos_fill;
			std::clog << "Bytes read = " << bytes_read << std::endl;
			std::clog << "Declared size = " << host_format(declared_size()) << std::endl;
			return bytes_read;
		}

		template<class S>
		ssize_t flush(S sink) {
			ssize_t bytes_written = 0;
			T* chunk = _contents.front();
			Size decl_sz = host_format(declared_size());
			while (!_contents.empty()
				&& (bytes_written = sink.write(chunk + _read_pos_flush, (last_chunk() ? _write_pos : _chunk_size) - _read_pos_flush)) > 0)
			{
				std::clog << "Bytes written = " << bytes_written << std::endl;
				_read_pos_flush += bytes_written;
				if (_read_pos_flush == _chunk_size) {
					delete[] chunk;
					_contents.pop_front();
					chunk = _contents.front();
					_read_pos_flush = 0;
				}
			}
			_read_pos = _read_pos_flush;
			std::clog << "Bytes written = " << bytes_written << std::endl;
			std::clog << "Declared size = " << decl_sz << std::endl;
			return bytes_written;
		}

		bool is_flushed() const { return _contents.empty(); }
		Size write_pos() const { return _write_pos + (_contents.size() - 1) * _chunk_size; }

		void declared_size(Size size) {
			T* chunk = _contents.front();
			T* st = reinterpret_cast<T*>(&size); 
			std::copy(st, st + sizeof size, chunk);
		}

		Size declared_size() const {
			Size size = 0;
			if (!_contents.empty()) {
				T* chunk = _contents.front();
				T* st = reinterpret_cast<T*>(&size); 
				std::copy(chunk, chunk + sizeof size, st);
			}
			return size;
		}

		friend std::ostream& operator<<(std::ostream& out, const Buffer& rhs) {
			out << std::hex << std::setfill('0');
			if (rhs._contents.size() > 1) {
				std::for_each(rhs._contents.cbegin(), rhs._contents.cend()-1, [&rhs, &out] (T* chunk) {
					for (size_t i=0; i<rhs._chunk_size; ++i) {
						out << std::setw(2) << (unsigned int)(unsigned char)chunk[i];
					}
				});
			}
			if (!rhs._contents.empty()) {
				T* chunk = rhs._contents.back();
				size_t n = std::max(rhs._write_pos, rhs._read_pos);
				for (size_t i=0; i<n; ++i) {
					out << std::setw(2) << (unsigned int)(unsigned char)chunk[i];
				}
			}
			out << std::dec << std::setfill(' ');
			return out;
		}
	
	private:

		bool last_chunk() const { return _contents.size() == 1; }

		Size _chunk_size;
		std::deque<T*> _contents;
		Size _read_pos;
		Size _write_pos;
		Size _read_pos_flush;
		Size _write_pos_fill;

		static const Size RESERVED_SPACE = sizeof(Size);
	};

}
