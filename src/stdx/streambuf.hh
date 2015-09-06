#ifndef STDX_STREAMBUF_HH
#define STDX_STREAMBUF_HH

#include <streambuf>

namespace stdx {

	template<class Base>
	struct pipebuf: public Base {

		typedef Base base_type;
		using typename base_type::char_type;
		using typename base_type::traits_type;

		template<class Source>
		void
		fill_from(Source& src) {
			std::streamsize n = 0;
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
			std::streamsize n = 0;
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

		std::streamsize
		pubfill() {
			return fill();
		}

	protected:
		
		virtual std::streamsize
		fill() {
			return 0;
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

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_streambuf: public pipebuf<std::basic_streambuf<Ch,Tr>>
	{};

}

#endif // STDX_STREAMBUF_HH
