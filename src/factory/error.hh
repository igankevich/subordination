namespace factory {

	namespace components {

		enum struct Level {
			KERNEL,
			SERVER,
			HANDLER,
			COMPONENT,
			STRATEGY,
			DISCOVERY,
			GRAPH,
			WEBSOCKET,
			TEST,
			IO,
			SHMEM,
			APP,
			ERR
		};

		constexpr
		const char* to_string(Level rhs) {
			return
				rhs == Level::KERNEL    ? "krnl"    : 
				rhs == Level::SERVER    ? "srvr"    : 
				rhs == Level::HANDLER   ? "handler" : 
				rhs == Level::COMPONENT ? "cmpnt"   : 
				rhs == Level::STRATEGY  ? "strat"   : 
				rhs == Level::DISCOVERY ? "dscvr"   : 
				rhs == Level::GRAPH     ? "grph"    : 
				rhs == Level::WEBSOCKET ? "wbsckt"  : 
				rhs == Level::TEST      ? "tst"     : 
				rhs == Level::IO        ? "io"      : 
				rhs == Level::SHMEM     ? "shm"     : 
				rhs == Level::APP       ? "app"     : 
				rhs == Level::ERR       ? "err"     : 
				"unknwn";
		}

		extern spin_mutex __logger_mutex;

		template<Level lvl=Level::KERNEL>
		struct Logger {

			inline
			Logger(): _buf() {
				__logger_mutex.lock();
				this->next_record();
			}

			inline
			~Logger() {
				__logger_mutex.unlock();
			}

			Logger(const Logger&) = delete;
			Logger& operator=(const Logger&) = delete;
		
			template<class T>
			Logger& operator<<(const T& rhs) {
				this->_buf << rhs;
				return *this;
			}
		
			Logger& operator<<(std::ostream& ( *pf )(std::ostream&)) {
				this->_buf << pf;
				if (pf == static_cast<std::ostream& (*)(std::ostream&)>(&std::endl)) {
					std::cout << this->_buf.rdbuf() << std::flush;
					this->_buf.clear();
					this->next_record();
				}
				return *this;
			}

		private:

			const char* now() {
				std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				std::strftime(this->formatted_time, sizeof(formatted_time),
					"%FT%T%z", std::localtime(&now_time));
				return this->formatted_time;
			}

			void next_record() {
				this->_buf << this->now() << SEP;
				components::print_all_endpoints(this->_buf);
				this->_buf << SEP;
				this->_buf << this_process::id() << SEP;
				this->_buf << std::this_thread::get_id() << SEP;
				this->_buf << to_string(lvl) << SEP;
			}

			std::stringstream _buf;
			char formatted_time[25];
			
			static const char SEP = ' ';
		};

		struct No_logger {

			constexpr No_logger() = default;

			No_logger(const No_logger&) = delete;
			No_logger& operator=(const No_logger&) = delete;
		
			template<class T>
			constexpr const No_logger&
			operator<<(const T&) const { return *this; }

			constexpr const No_logger&
			operator<<(std::ostream& ( *pf )(std::ostream&)) const { return *this; }
		};

//		template<> struct Logger<Level::KERNEL   >: public No_logger {};
//		template<> struct Logger<Level::SERVER   >: public No_logger {};
		template<> struct Logger<Level::HANDLER  >: public No_logger {};
		template<> struct Logger<Level::IO       >: public No_logger {};
//		template<> struct Logger<Level::COMPONENT>: public No_logger {};
//		template<> struct Logger<Level::STRATEGY >: public No_logger {};
//		template<> struct Logger<Level::DISCOVERY>: public No_logger {};
//		template<> struct Logger<Level::GRAPH    >: public No_logger {};
//		template<> struct Logger<Level::WEBSOCKET>: public No_logger {};

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

			Error(const std::string& msg, const char* file, const int line, const char* function) noexcept:
				std::runtime_error(msg), _file(file), _line(line), _function(function)
			{}

			Error(const Error& rhs) noexcept:
				std::runtime_error(rhs), _file(rhs._file), _line(rhs._line), _function(rhs._function) {}

			Error& operator=(const Error&) = delete;

			friend std::ostream& operator<<(std::ostream& out, const Error& rhs) {
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
		Ret
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
			Connection_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Read_error: public Error {
			Read_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Write_error: public Error {
			Write_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Marshalling_error: public Error {
			Marshalling_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Durability_error: public Error {
			Durability_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Network_error: public Error {
			Network_error(const std::string& msg, const char* file, const int line, const char* function):
				Error(msg, file, line, function) {}
		};

		struct Error_message {

			constexpr Error_message(const Error& err, const char* file, const int line, const char* function):
				_error(err), _file(file), _line(line), _function(function) {}

			friend std::ostream& operator<<(std::ostream& out, const Error_message& rhs) {
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

	typedef components::Level Level;
	template<Level lvl> using Logger = components::Logger<lvl>;


	enum struct Result: uint16_t {
		SUCCESS = 0,
		UNDEFINED = 1,
		ENDPOINT_NOT_CONNECTED = 3,
		NO_UPSTREAM_SERVERS_LEFT = 4,
		NO_PRINCIPAL_FOUND = 5,
		USER_ERROR = 6,
		FATAL_ERROR = 7
	};

	inline
	std::ostream& operator<<(std::ostream& out, Result rhs) {
		switch (rhs) {
			case Result::SUCCESS: out << "SUCCESS"; break;
			case Result::UNDEFINED: out << "UNDEFINED"; break;
			case Result::ENDPOINT_NOT_CONNECTED: out << "ENDPOINT_NOT_CONNECTED"; break;
			case Result::NO_UPSTREAM_SERVERS_LEFT: out << "NO_UPSTREAM_SERVERS_LEFT"; break;
			case Result::NO_PRINCIPAL_FOUND: out << "NO_PRINCIPAL_FOUND"; break;
			case Result::USER_ERROR: out << "USER_ERROR"; break;
			case Result::FATAL_ERROR: out << "FATAL_ERROR"; break;
			default: out << "UNKNOWN_RESULT";
		}
		return out;
	}

	namespace components {

		struct debug_stream {
			constexpr explicit debug_stream(const std::ios& s): str(s) {}
			friend std::ostream& operator<<(std::ostream& out, const debug_stream& rhs) {
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

}
