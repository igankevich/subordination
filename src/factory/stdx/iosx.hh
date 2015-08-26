#ifndef FACTORY_STDX_IOSX_HH
#define FACTORY_STDX_IOSX_HH

#include <ostream>

namespace factory {

	namespace stdx {

		template<class T, std::streamsize W>
		struct fixed_width {
			fixed_width(T x): val(x) {}
			friend std::ostream&
			operator<<(std::ostream& out, const fixed_width& rhs) {
				return out << std::setw(W) << rhs.val;
			}
		private:
			T val;
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

	}

}

#endif // FACTORY_STDX_IOSX_HH
