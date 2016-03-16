#ifndef FACTORY_ERROR_HH
#define FACTORY_ERROR_HH

#include <stdexcept>

namespace std {

	std::ostream&
	operator<<(std::ostream& out, const std::exception& rhs) {
		return out << rhs.what();
	}

}

namespace factory {

	namespace components {

		struct Error_location {

			Error_location(const char* file, const int line, const char* func) noexcept:
			_file(file),
			_line(line),
			_func(func)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Error_location& rhs) {
				return
				out << rhs._file << ':'
					<< rhs._line << ':'
					<< rhs._func;
			}

		private:

			const char* _file;
			const int _line;
			const char* _func;

		};

		struct Error: public std::runtime_error {

			Error(std::string&& msg, const char* file, const int line, const char* function) noexcept:
			std::runtime_error(std::forward<std::string>(msg)),
			_location{file, line, function}
			{}

			Error(const char* msg, const Error_location& loc) noexcept:
			std::runtime_error(msg),
			_location(loc)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Error& rhs) {
				return out << rhs._location << ' ' << rhs.what();
			}

		private:

			const Error_location& _location;

		};

		struct Marshalling_error: public Error {
			Marshalling_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Bad_type: public Error {
			Bad_type(const char* msg, const Error_location& loc) noexcept:
			Error(msg, loc) {}
		};

	}

}

#endif // FACTORY_ERROR_HH
