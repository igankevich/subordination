#ifndef STDX_IOSX_HH
#define STDX_IOSX_HH

#include <ostream>

namespace stdx {

	template<class T, std::streamsize W>
	struct fixed_width {
		fixed_width(const T& x): val(x) {}
		friend std::ostream&
		operator<<(std::ostream& out, const fixed_width& rhs) {
			out.width(W);
			return out << rhs.val;
		}
	private:
		const T& val;
	};

	struct ios_guard {
		explicit
		ios_guard(std::ios& s):
			str(s), oldf(str.flags()),
			oldfill(str.fill()) {}
		~ios_guard() {
			str.setf(oldf);
			str.fill(oldfill);
		}
	private:
		std::ios& str;
		std::ios_base::fmtflags oldf;
		std::ios::char_type oldfill;
	};

	template<class T>
	struct debug_stream_t {

		constexpr explicit
		debug_stream_t(const T& s) noexcept:
		str(s) {}

		friend std::ostream&
		operator<<(std::ostream& out, const debug_stream_t& rhs) {
			//std::ostream::sentry s(out);
			//if (s) {
			//	out
			//		<< (rhs.str.good() ? 'g' : '-')
			//		<< (rhs.str.bad()  ? 'b' : '-')
			//		<< (rhs.str.fail() ? 'f' : '-')
			//		<< (rhs.str.eof()  ? 'e' : '-');
			//}
			return out;
		}

	private:

		const T& str;

	};

	template<class T>
	constexpr debug_stream_t<T>
	debug_stream(const T& str) {
		return debug_stream_t<T>(str);
	}

}

#endif // STDX_IOSX_HH
