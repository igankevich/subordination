#ifndef SYSX_FDBUF_HH
#define SYSX_FDBUF_HH

#include <vector>
#include <cassert>

#include <sysx/bits/buffer_category.hh>
#include <sysx/fildes.hh>
#include <stdx/packetbuf.hh>
#include <stdx/log.hh>

namespace sysx {

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
		struct basic_fdbuf: public stdx::basic_packetbuf<Ch,Tr> {

			typedef stdx::basic_packetbuf<Ch> base_type;
			using base_type::gptr;
			using base_type::eback;
			using base_type::egptr;
			using base_type::pptr;
			using base_type::pbase;
			using base_type::epptr;
			using base_type::setg;
			using base_type::setp;
			using base_type::gbump;
			using base_type::pbump;
			using typename base_type::int_type;
			using typename base_type::traits_type;
			using typename base_type::char_type;
			using typename base_type::pos_type;
			using typename base_type::off_type;
			typedef std::ios_base::openmode openmode;
			typedef std::ios_base::seekdir seekdir;
			typedef Fd fd_type;
			typedef stdx::log<basic_fdbuf> this_log;
			typedef typename std::vector<char_type>::size_type size_type;

			basic_fdbuf(): basic_fdbuf(std::move(fd_type()), 512, 512) {}

			basic_fdbuf(fd_type&& fd, size_type gbufsize, size_type pbufsize):
			_fd(std::move(fd)), _gbuf(gbufsize), _pbuf(pbufsize)
			{
				char_type* end = _gbuf.data();
				setg(end, end, end);
				char_type* beg = _pbuf.data();
				setp(beg, beg + _pbuf.size());
			}

			basic_fdbuf(basic_fdbuf&& rhs) = default;

			virtual
			~basic_fdbuf() {
				// NB: for now full sync is not guaranteed for non-blocking I/O
				// as it may result in infinite loop
				sync();
			}

			int_type
			underflow() override {
				assert(gptr() == egptr());
				return fill_from_fd(0) == 0
					? traits_type::eof()
					: traits_type::to_int_type(*gptr());
			}

			int_type
			overflow(int_type c) override {
				assert(pptr() == epptr());
				if (c != traits_type::eof()) {
					pgrow();
					if (pptr() != epptr()) {
						*pptr() = c;
						pbump(1);
					}
				} else {
					sync();
				}
				return c;
			}

			std::streamsize
			fill() override {
				return fill_from_fd(gptr()-eback());
			}

			int
			sync() override {
				const std::streamsize m = pptr() - pbase();
				const std::streamsize n = _fd.write(pbase(), m);
				if (n > 0) {
					pbump(-n);
				}
				return n == m ? 0 : -1;
			}

			pos_type
			seekoff(off_type off, seekdir way, openmode which) override {
				pos_type ret(off_type(-1));
				if (way == std::ios_base::beg) {
					this_log() << "seekoff way=beg,off=" << off << std::endl;
					ret = seekpos(off, which);
				}
				if (way == std::ios_base::cur) {
					this_log() << "seekoff way=cur,off=" << off << std::endl;
					const pos_type pos = which & std::ios_base::in
						? static_cast<pos_type>(gptr() - eback())
						: static_cast<pos_type>(pptr() - pbase());
					ret = off == 0 ? pos : seekpos(pos + off, which);
				}
				if (way == std::ios_base::end) {
					this_log() << "seekoff way=end,off=" << off << std::endl;
					const pos_type pos = which & std::ios_base::in
						? static_cast<pos_type>(egptr() - eback())
						: static_cast<pos_type>(epptr() - pbase());
					ret = seekpos(pos + off, which);
				}
				return ret;
			}

			pos_type
			seekpos(pos_type pos, openmode mode) override {
				pos_type ret(off_type(-1));
				if (mode & std::ios_base::in) {
					const std::streamsize size = egptr() - eback();
					if (pos >= 0 && pos <= size) {
						setg(eback(), eback()+pos, egptr());
						ret = pos;
					}
				}
				if (mode & std::ios_base::out) {
					if (pos >= 0 && pos <= psize()) {
						setp(pbase(), epptr());
						pbump(pos);
						ret = pos;
					}
				}
				return ret;
			}

			void
			setfd(fd_type&& rhs) {
				_fd = std::move(rhs);
			}

			const fd_type&
			fd() const {
				return _fd;
			}

			fd_type&
			fd() {
				return _fd;
			}
		
		private:

			std::streamsize
			fill_from_fd(std::streamsize offset) {
				std::streamsize n = 0, nread = offset;
				while ((n = _fd.read(_gbuf.data() + nread, _gbuf.size() - nread)) > 0) {
					nread += n;
					if (nread == _gbuf.size()) {
						ggrow();
					}
				}
				char_type* base = _gbuf.data();
				setg(base, base + offset, base + nread);
				return nread - offset;
			}

		protected:

			void
			ggrow() {
				_gbuf.resize(_gbuf.size() * 2);
			}

			void
			pgrow() {
				const pos_type off = pptr() - pbase();
				const pos_type n = epptr() - pbase();
				_pbuf.resize(_pbuf.size() * 2);
				char_type* base = _pbuf.data();
				setp(base, base + _pbuf.size());
				pbump(off);
			}

			size_type
			psize() const {
				return _pbuf.size();
			}

			fd_type _fd;
			std::vector<char_type> _gbuf;
			std::vector<char_type> _pbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
		struct basic_ifdstream: public std::basic_istream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_istream<Ch,Tr> istream_type;
			explicit basic_ifdstream(Fd&& fd): istream_type(nullptr),
				_fdbuf(std::move(fd), 512, 0) { this->init(&_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
		struct basic_ofdstream: public std::basic_ostream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_ostream<Ch,Tr> ostream_type;
			explicit basic_ofdstream(Fd&& fd): ostream_type(nullptr),
				_fdbuf(std::move(fd), 0, 512) { this->init(&_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

		template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
		struct basic_fdstream: public std::basic_iostream<Ch> {
			typedef basic_fdbuf<Ch,Fd> fdbuf_type;
			typedef std::basic_iostream<Ch,Tr> iostream_type;
			explicit basic_fdstream(Fd&& fd): iostream_type(nullptr),
				_fdbuf(std::move(fd), 512, 512) { this->init(&_fdbuf); }
		private:
			fdbuf_type _fdbuf;
		};

	typedef basic_fdbuf<char> fdbuf;
	typedef basic_ifdstream<char> ifdstream;
	typedef basic_ofdstream<char> ofdstream;

}

namespace stdx {

	template<class T, class Fd>
	struct type_traits<sysx::basic_fdbuf<T,Fd>> {
		static constexpr const char*
		short_name() { return "fdbuf"; }
		typedef sysx::buffer_category category;
	};

}

#endif // SYSX_FDBUF_HH
