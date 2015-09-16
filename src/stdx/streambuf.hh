#ifndef STDX_STREAMBUF_HH
#define STDX_STREAMBUF_HH

#include <streambuf>

namespace stdx {

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_streambuf: public std::basic_streambuf<Ch,Tr> {

		typedef std::basic_streambuf<Ch,Tr> base_type;
		using typename base_type::char_type;
		using typename base_type::traits_type;
		using typename base_type::pos_type;
		using base_type::setg;
		using base_type::setp;
		using base_type::pbump;
		using base_type::pbase;
		using base_type::pptr;
		using base_type::epptr;

		// TODO: delete this for newer versions of stdc++
		basic_streambuf() = default;
		basic_streambuf(basic_streambuf&& rhs) {
			setg(rhs.eback(), rhs.gptr(), rhs.egptr());
			setp(rhs.pbase(), rhs.epptr());
			pbump(rhs.pptr()-rhs.pbase());
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

	};

	typedef basic_streambuf<char> streambuf;

}

#endif // STDX_STREAMBUF_HH
