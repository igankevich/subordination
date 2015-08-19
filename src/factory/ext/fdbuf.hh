namespace factory {

	namespace components {

		template<class T, class Fd=unix::fd>
		struct basic_fdbuf: public std::basic_streambuf<T> {

			using typename std::basic_streambuf<T>::int_type;
			using typename std::basic_streambuf<T>::traits_type;
			using typename std::basic_streambuf<T>::char_type;
			using typename std::basic_streambuf<T>::pos_type;
			using typename std::basic_streambuf<T>::off_type;
			typedef std::ios_base::openmode openmode;
			typedef std::ios_base::seekdir seekdir;
			typedef Fd fd_type;

			static fd_type&& badfd() {
//				return std::move(std::is_same<fd_type,int>::value ? fd_type(-1) : fd_type());
				return std::move(fd_type());
			}

			basic_fdbuf(): basic_fdbuf(std::move(badfd()), 512, 512) {}

			basic_fdbuf(fd_type&& fd, std::size_t gbufsize, std::size_t pbufsize):
				_fd(std::move(fd)),
				_gbuf(gbufsize),
				_pbuf(pbufsize)
			{
				char_type* end = &_gbuf.front();
				this->setg(end, end, end);
				char_type* beg = &this->_pbuf.front();
				this->setp(beg, beg + this->_pbuf.size());
			}


//			basic_fdbuf& operator=(basic_fdbuf&) = delete;
//			basic_fdbuf(basic_fdbuf&) = delete;
			basic_fdbuf(basic_fdbuf&& rhs) = default;

			virtual ~basic_fdbuf() {
				// TODO: for now full sync is not guaranteed for non-blocking I/O
				// as it may result in infinite loop
				this->sync();
			}

			int_type underflow() {
				if (this->gptr() != this->egptr()) {
					return traits_type::to_int_type(*this->gptr());
				}
//				if (this->eback() == this->gptr()) {
//					char_type* base = &this->_gbuf.front();
//					ssize_t n = this->_fd.read(base, this->_gbuf.size());
//					Logger<Level::IO>() << "Reading fd=" << _fd << ",n=" << n << std::endl;
//					if (n <= 0) {
//						return traits_type::eof();
//					}
//					this->setg(base, base, base + n);
//				} else
				{
//					this->growgbuf(this->_gbuf.size() * 2);
					ssize_t n = 0;
					pos_type old_pos = this->gptr() - this->eback();
					pos_type pos = this->egptr() - this->eback();
					while ((n = this->_fd.read(this->eback() + pos, this->_gbuf.size() - pos)) > 0) {
						pos += n;
						if (pos == this->_gbuf.size()) {
							this->growgbuf(this->_gbuf.size() * 2);
						}
					}
					if (pos == old_pos) {
						return traits_type::eof();
					}
					char_type* base = this->eback();
					this->setg(base, base + old_pos, base + pos);
				}
				return traits_type::to_int_type(*this->gptr());
			}

			int_type overflow(int_type c = traits_type::eof()) {
				if (c != traits_type::eof()) {
					if (this->pptr() == this->epptr()) {
						this->growpbuf(this->_pbuf.size() * 2);
					// TODO: do we need sync???
//						if (this->sync() == -1) {
//						}
					}
					if (this->pptr() != this->epptr()) {
						*this->pptr() = c;
						this->pbump(1);
						return traits_type::to_int_type(c);
					}
				} else {
					// TODO: do we need sync???
//					this->sync();
				}
				return traits_type::eof();
			}

			int sync() {
				Logger<Level::IO>() << "Sync" << std::endl;
				if (this->pptr() == this->pbase()) return 0;
				ssize_t n = this->_fd.write(this->pbase(), this->pptr() - this->pbase());
				if (n <= 0) {
					return -1;
				}
				this->pbump(-n);
				Logger<Level::IO>() << "Writing fd=" << this->_fd << ",n=" << n << std::endl;
				return this->pptr() == this->pbase() ? 0 : -1;
			}

			pos_type seekoff(off_type off, seekdir way,
				openmode which = std::ios_base::in | std::ios_base::out)
			{
				if (way == std::ios_base::beg) {
					Logger<Level::IO>() << "seekoff way=beg,off=" << off << std::endl;
					return this->seekpos(off, which);
				}
				if (way == std::ios_base::cur) {
					Logger<Level::IO>() << "seekoff way=cur,off=" << off << std::endl;
					pos_type pos = which & std::ios_base::in
						? static_cast<pos_type>(this->gptr() - this->eback())
						: static_cast<pos_type>(this->pptr() - this->pbase());
					return off == 0 ? pos 
						: this->seekpos(pos + off, which);
				}
				if (way == std::ios_base::end) {
					Logger<Level::IO>() << "seekoff way=end,off=" << off << std::endl;
					pos_type pos = which & std::ios_base::in
						? static_cast<pos_type>(this->egptr() - this->eback())
						: static_cast<pos_type>(this->epptr() - this->pbase());
					return this->seekpos(pos + off, which);
				}
				return pos_type(off_type(-1));
			}

			pos_type seekpos(pos_type pos,
				openmode mode = std::ios_base::in | std::ios_base::out)
			{
				Logger<Level::IO>() << "seekpos " << pos << std::endl;
				if (mode & std::ios_base::in) {
					std::size_t size = this->egptr() - this->eback();
					if (pos >= 0 && pos <= size) {
						char_type* beg = this->eback();
						char_type* end = this->egptr();
						char_type* xgptr = beg+pos;
						this->setg(beg, xgptr, end);
					}
					// always return current position
					return static_cast<pos_type>(this->gptr() - this->eback());
				}
				if (mode & std::ios_base::out) {
					std::size_t size = this->_pbuf.size();
					if (pos >= 0 && pos <= size) {
						std::ptrdiff_t off = this->pptr() - this->pbase();
						this->pbump(static_cast<std::ptrdiff_t>(pos)-off);
					}
//					// enlarge buffer
//					if (pos > size) {
//						Logger<Level::IO>() << "GROW: pos=" << pos << std::endl;
//						this->growpbuf(pos);
//						this->pbump(this->epptr() - this->pptr());
//					}
					// always return current position
					return static_cast<pos_type>(this->pptr() - this->pbase());
				}
				return pos_type(off_type(-1));
			}

//			void setfd(int rhs) { this->_fd = rhs; }
			void setfd(fd_type&& rhs) { this->_fd = std::move(rhs); }
			const fd_type& fd() const { return this->_fd; }
			fd_type& fd() { return this->_fd; }
		
		private:
			static
			std::size_t calc_size(std::size_t target_size, std::size_t base_size) {
				while (base_size < target_size
					&& base_size <= std::numeric_limits<std::size_t>::max()/2)
				{
					base_size *= 2;
				}
				return base_size;
			}

		protected:

			void growgbuf(std::size_t target_size) {
				if (target_size <= this->_gbuf.size()) return;
				std::size_t size = this->_gbuf.size();
				std::size_t new_size = calc_size(target_size, size);
				std::ptrdiff_t off = this->gptr() - this->eback();
				std::ptrdiff_t n = this->egptr() - this->eback();
				this->_gbuf.resize(new_size);
				Logger<Level::IO>() << "Resize gbuf size="
					<< this->_gbuf.size() << std::endl;
				char_type* base = &this->_gbuf.front();
				this->setg(base, base + off, base + n);
			}

			void growpbuf(std::size_t target_size) {
				if (target_size <= this->_pbuf.size()) return;
				std::size_t size = this->_pbuf.size();
				std::size_t new_size = calc_size(target_size, size);
				std::ptrdiff_t off = this->pptr() - this->pbase();
				std::ptrdiff_t n = this->epptr() - this->pbase();
				this->_pbuf.resize(new_size);
				Logger<Level::IO>() << "Resize pbuf size=" << new_size << std::endl;
				char_type* base = &this->_pbuf.front();
				this->setp(base, base + new_size);
				this->pbump(off);
			}

			std::size_t pbufsize() const { return this->_pbuf.size(); }

			fd_type _fd;
			std::vector<char_type> _gbuf;
			std::vector<char_type> _pbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=unix::fd>
		struct basic_ifdstream: public std::basic_istream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_istream<Ch,Tr> istream_type;
			explicit basic_ifdstream(Fd&& fd): istream_type(nullptr),
				_fdbuf(std::move(fd), 512, 0) { this->init(&this->_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=unix::fd>
		struct basic_ofdstream: public std::basic_ostream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_ostream<Ch,Tr> ostream_type;
			explicit basic_ofdstream(Fd&& fd): ostream_type(nullptr),
				_fdbuf(std::move(fd), 0, 512) { this->init(&this->_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=unix::fd>
		struct basic_fdstream: public std::basic_iostream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_iostream<Ch,Tr> iostream_type;
			explicit basic_fdstream(Fd&& fd): iostream_type(nullptr),
				_fdbuf(std::move(fd), 512, 512) { this->init(&this->_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

	}

	typedef components::basic_fdbuf<char> fdbuf;
	typedef components::basic_ifdstream<char> ifdstream;
	typedef components::basic_ofdstream<char> ofdstream;

}
