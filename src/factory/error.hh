namespace factory {

	namespace components {

		struct Backtrace {
			Backtrace();
			~Backtrace() { ::free(this->symbols); }
			Backtrace(const Backtrace&) = delete;
			Backtrace& operator=(const Backtrace&) = delete;

			friend std::ostream& operator<<(std::ostream& out, const Backtrace& rhs);

		private:
			backtrace_size_t num_entries = 0;
			char** symbols = nullptr;
			static const backtrace_size_t MAX_ENTRIES = 64;
		};

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
					<< ",backtrace=" << rhs._backtrace
					<< '}';
			}

		private:
			const char* _file;
			const int   _line;
			const char* _function;
			Backtrace _backtrace;
		};

		template<class Ret>
		inline Ret
		check(Ret ret, Ret bad, const char* file, const int line, const char* func) {
			if (ret == bad) {
				throw Error(std::system_error(std::error_code(errno,
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
					<< ",rev=" << REPO_VERSION
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
				out << std::chrono::system_clock::now().time_since_epoch().count() << ' '
					<< rhs._function << '[' << rhs._file << ':' << rhs._line << ']'
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
