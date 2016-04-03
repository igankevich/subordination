#ifndef STDX_STREAMBUF_HH
#define STDX_STREAMBUF_HH

#include <streambuf>

namespace stdx {

	template<class T>
	struct streambuf_traits {

		typedef typename T::char_type char_type;

		static std::streamsize
		write(T& buf, const char_type* s, std::streamsize n) {
			return buf.sputn(s, n);
		}

		static std::streamsize
		read(T& buf, char_type* s, std::streamsize n) {
			return buf.sgetn(s, n);
		}

	};


}

#endif // STDX_STREAMBUF_HH
