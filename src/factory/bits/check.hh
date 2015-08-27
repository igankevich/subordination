#ifndef FACTORY_BITS_CHECK_HH
#define FACTORY_BITS_CHECK_HH

#include <stdexcept>
#include <system_error>
#include <ostream>

namespace factory {

	namespace bits {

		struct os_error: public std::runtime_error {

			os_error(const std::runtime_error& err, const char* file, const int line, const char* function) noexcept:
				std::runtime_error(err), _file(file), _line(line), _function(function)
			{}

			os_error(std::string&& msg, const char* file, const int line, const char* function) noexcept:
			std::runtime_error(std::forward<std::string>(msg)),
			_file(file),
			_line(line),
			_function(function)
			{}

			os_error(const os_error& rhs) noexcept:
			std::runtime_error(rhs),
			_file(rhs._file),
			_line(rhs._line),
			_function(rhs._function)
			{}

			os_error& operator=(const os_error&) = delete;

			friend std::ostream&
			operator<<(std::ostream& out, const os_error& rhs) {
				return out << '{'
					<< "what=" << rhs.what()
					<< ",origin=" << rhs._function
					<< '[' << rhs._file << ':' << rhs._line << ']'
					<< '}';
			}

		private:
			const char* _file;
			const int   _line;
			const char* _function;
		};

		template<class Ret>
		inline Ret
		check(Ret ret, Ret bad, const char* file, const int line, const char* func) {
			if (ret == bad) {
				throw os_error(std::system_error(std::error_code(errno,
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
