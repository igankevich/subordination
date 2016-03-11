#ifndef SYSX_BITS_TO_STRING_HH
#define SYSX_BITS_TO_STRING_HH

#include <sstream>

namespace sysx {

	namespace bits {

		template<class T>
		std::string
		to_string(T rhs) {
			std::stringstream s;
			s << rhs;
			return s.str();
		}

		/// @deprecated too inefficient
		struct To_string {

			template<class T>
			inline
			To_string(T rhs):
			_s(to_string(rhs)) {}

			const char*
			c_str() const noexcept {
				return _s.c_str();
			}

		private:
			std::string _s;
		};

	}

}

#endif // SYSX_BITS_TO_STRING_HH
