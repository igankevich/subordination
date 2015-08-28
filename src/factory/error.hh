#ifndef FACTORY_ERROR_HH
#define FACTORY_ERROR_HH

#include <stdexcept>

namespace factory {

	namespace components {

		struct Error: public std::runtime_error {

			Error(const std::runtime_error& err, const char* file, const int line, const char* function) noexcept:
				std::runtime_error(err), _file(file), _line(line), _function(function)
			{}

			Error(std::string&& msg, const char* file, const int line, const char* function) noexcept:
			std::runtime_error(std::forward<std::string>(msg)),
			_file(file),
			_line(line),
			_function(function)
			{}

			Error(const Error& rhs) noexcept:
			std::runtime_error(rhs),
			_file(rhs._file),
			_line(rhs._line),
			_function(rhs._function)
			{}

			Error& operator=(const Error&) = delete;

			friend std::ostream&
			operator<<(std::ostream& out, const Error& rhs) {
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

		struct Connection_error: public Error {
			Connection_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Read_error: public Error {
			Read_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Write_error: public Error {
			Write_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Marshalling_error: public Error {
			Marshalling_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Durability_error: public Error {
			Durability_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Network_error: public Error {
			Network_error(std::string&& msg, const char* file, const int line, const char* function):
				Error(std::forward<std::string>(msg), file, line, function) {}
		};

		struct Error_message {

			constexpr
			Error_message(const Error& err, const char* file, const int line, const char* function) noexcept:
				_error(err), _file(file), _line(line), _function(function) {}

			friend std::ostream&
			operator<<(std::ostream& out, const Error_message& rhs) {
				return out << '{'
					<< "caught_at=" << rhs._function << '[' << rhs._file << ':' << rhs._line << ']'
					<< ",err=" << rhs._error
					<< '}';
			}

		private:
			const Error& _error;
			const char* _file;
			const int   _line;
			const char* _function;
		};

		struct String_message {

			constexpr String_message(const char* err, const char* file, const int line, const char* function):
				_error(err), _file(file), _line(line), _function(function) {}

			String_message(const std::exception& err, const char* file, const int line, const char* func):
				_error(err.what()), _file(file), _line(line), _function(func) {}

			friend std::ostream& operator<<(std::ostream& out, const String_message& rhs) {
				out << rhs._function << '[' << rhs._file << ':' << rhs._line << ']'
					<< ' ' << rhs._error << std::endl;
				return out;
			}

		private:
			const char* _error;
			const char* _file;
			const int   _line;
			const char* _function;
		};

		constexpr const char* UNKNOWN_ERROR = "Unknown error.";

		template<class K>
		struct No_principal_found {
			constexpr explicit No_principal_found(K* k): _kernel(k) {}
			K* kernel() { return _kernel; }
		private:
			K* _kernel;
		};

	}

}

#endif // FACTORY_ERROR_HH
