#ifndef FACTORY_STDX_IOSX_HH
#define FACTORY_STDX_IOSX_HH

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

	struct debug_stream {

		constexpr explicit
		debug_stream(const std::ios& s) noexcept:
		str(s) {}

		friend std::ostream&
		operator<<(std::ostream& out, const debug_stream& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				out
					<< (rhs.str.good() ? 'g' : '-')
					<< (rhs.str.bad()  ? 'b' : '-')
					<< (rhs.str.fail() ? 'f' : '-')
					<< (rhs.str.eof()  ? 'e' : '-');
			}
			return out;
		}

	private:

		const std::ios& str;

	};

}

#endif // FACTORY_STDX_IOSX_HH
