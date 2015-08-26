#ifndef FACTORY_BITS_CHECK_HH
#define FACTORY_BITS_CHECK_HH

#include <system_error>

namespace factory {

	namespace bits {

		template<class Ret>
		inline Ret
		check(Ret ret, Ret bad, const char* file, const int line, const char* func) {
			if (ret == bad) {
				throw components::Error(std::system_error(std::error_code(errno,
					std::generic_category()), func),
					file, line, func);
			}
			return ret;
		}

		template<class Ret>
		inline Ret
		check(Ret ret, const char* file, const int line, const char* func) {
			return check<Ret>(ret, Ret(-1), file, line, func);
		}

		template<std::errc ignored_errcode, class Ret>
		inline
		Ret check_if_not(Ret ret, const char* file, const int line, const char* func) {
			std::error_code errcode(errno, std::generic_category());
			return errcode == std::make_error_code(ignored_errcode)
				? Ret(0)
				: check<Ret>(ret, file, line, func);
		}

	}

}

#endif // FACTORY_BITS_CHECK_HH
