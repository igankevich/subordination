#ifndef FACTORY_BITS_TO_STRING_HH
#define FACTORY_BITS_TO_STRING_HH

namespace sysx {

	namespace bits {

		template<class T>
		std::string
		to_string(T rhs) {
			std::stringstream s;
			s << rhs;
			return s.str();
		}

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

#endif // FACTORY_BITS_TO_STRING_HH
