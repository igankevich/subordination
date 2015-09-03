#ifndef STDX_STREAMBUF_HH
#define STDX_STREAMBUF_HH

#include <streambuf>

namespace stdx {

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_streambuf: public std::basic_streambuf<Ch,Tr> {

		typedef std::basic_streambuf<Ch,Tr> base_type;
		using typename base_type::char_type;
		using typename base_type::traits_type;

		template<class Source>
		void
		fill_from(Source& src) {
			ssize_t n = 0;
			while (true) {
				if (pfirst() == plast()) {
					pgrow();
				}
				n = src.sgetn(pfirst(), plast() - pfirst());
				std::cout << "Read " << n << std::endl;
				if (n <= 0) {
					break;
				}
				this->pbump(n);
			}
		}

		template<class Sink>
		void
		flush_to(Sink& sink) {
			ssize_t n = 0;
			while (true) {
				if (gfirst() == glast()) {
					this->underflow();
				}
				n = sink.sputn(gfirst(), glast() - gfirst());
				if (n <= 0) {
					break;
				}
				this->gbump(n);
			}
		}

	private:

		void
		pgrow() {
			this->overflow('x');
			this->pbump(-1);
		}

		char_type*
		pfirst() {
			return this->pptr();
		}

		char_type*
		plast() {
			return this->epptr();
		}

		char_type*
		gfirst() {
			return this->gptr();
		}

		char_type*
		glast() {
			return this->egptr();
		}

	};

}

#endif // STDX_STREAMBUF_HH
