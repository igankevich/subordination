namespace factory {

	enum struct Level: uint8_t {
		KERNEL,
		SERVER,
		HANDLER,
		COMPONENT,
		STRATEGY,
		DISCOVERY,
		GRAPH,
		WEBSOCKET,
		TEST
	};

	std::ostream& operator<<(std::ostream& out, const Level rhs) {
		switch (rhs) {
			case Level::KERNEL    : out << "krnl";    break;
			case Level::SERVER    : out << "srvr";    break;
			case Level::HANDLER   : out << "handler"; break;
			case Level::COMPONENT : out << "cmpnt";   break;
			case Level::STRATEGY  : out << "strat";   break;
			case Level::DISCOVERY : out << "dscvr";   break;
			case Level::GRAPH     : out << "grph";    break;
			case Level::WEBSOCKET : out << "wbsckt";  break;
			case Level::TEST      : out << "tst";     break;
			default               : out << "unknwn";  break;
		}
		return out;
	}

	Spin_mutex __logger_mutex;

	template<Level lvl=Level::KERNEL>
	struct Logger {

		Logger():
			_buf(),
			_tag(lvl)
		{
			next_record();
		}

		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;
	
		template<class T>
		Logger& operator<<(const T& rhs) {
			std::lock_guard<Spin_mutex> lock(__logger_mutex);
			_buf << rhs;
			return *this;
		}
	
		Logger& operator<<(std::ostream& ( *pf )(std::ostream&)) {
			_buf << pf;
			if (pf == static_cast<std::ostream& (*)(std::ostream&)>(&std::endl)) {
				{
					std::lock_guard<Spin_mutex> lock(__logger_mutex);
					std::cout << _buf.str() << std::flush;
				}
				_buf.str("");
				_buf.clear();
				next_record();
			}
			return *this;
		}

		std::ostream& ostream() { return _buf; }
	
	private:
		
		static Time now() {
			using namespace std::chrono;
			static Time start_time = current_time_nano();
			return current_time_nano() - start_time;
		}

		void next_record() {
			_buf << now() << SEP;
			components::factory_server_addr(_buf);
			_buf << SEP;
			_buf << this_process::id() << SEP;
			_buf << _tag << SEP;
		}

		std::stringstream _buf;
		Level _tag;
		
		static const char SEP = ' ';
	};

	struct No_stream: public std::ostream {
		No_stream(): std::ostream(&nobuf) {}
		
	private:
		struct No_buffer: public std::streambuf {
			int overflow(int c) const { return c; }
		};
		No_buffer nobuf;
	};

	struct No_logger {

		constexpr No_logger() {}

		No_logger(const No_logger&) = delete;
		No_logger& operator=(const No_logger&) = delete;
	
		template<class T>
		constexpr const No_logger&
		operator<<(const T&) const { return *this; }

		constexpr const No_logger&
		operator<<(std::ostream& ( *pf )(std::ostream&)) const { return *this; }

		std::ostream& ostream() const {
			static No_stream tmp;
			return tmp;
		}
	};

//	template<> struct Logger<Level::KERNEL   >: public No_logger {};
//	template<> struct Logger<Level::SERVER   >: public No_logger {};
	template<> struct Logger<Level::HANDLER  >: public No_logger {};
//	template<> struct Logger<Level::COMPONENT>: public No_logger {};
//	template<> struct Logger<Level::STRATEGY >: public No_logger {};
//	template<> struct Logger<Level::DISCOVERY>: public No_logger {};
////	template<> struct Logger<Level::GRAPH    >: public No_logger {};
//	template<> struct Logger<Level::WEBSOCKET>: public No_logger {};

	template<class Ret>
	Ret check(const char* func, Ret ret, Ret bad=Ret(-1)) {
		if (ret == bad) {
			throw std::system_error(std::error_code(errno, std::system_category()), func);
		}
		return ret;
	}

	struct Error: public std::runtime_error {

		Error(const std::string& msg, const char* file, const int line, const char* function):
			std::runtime_error(msg), _file(file), _line(line), _function(function)
		{}

		Error(const Error& rhs):
			std::runtime_error(rhs), _file(rhs._file), _line(rhs._line), _function(rhs._function) {}

		Error& operator=(const Error&) = delete;

		virtual const char* prefix() const { return "Error"; }

		friend std::ostream& operator<<(std::ostream& out, const Error& rhs) {
			return out
				<< "What:       " << rhs.what()
				<< "\nOrigin:     " << rhs._function
				<< '[' << rhs._file << ':' << rhs._line << ']';
		}

	private:
		const char* _file;
		const int   _line;
		const char* _function;
	};

	struct Connection_error: public Error {
		Connection_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Connection error"; }
	};

	struct Read_error: public Error {
		Read_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Read error"; }
	};

	struct Write_error: public Error {
		Write_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Write error"; }
	};

	struct Marshalling_error: public Error {
		Marshalling_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Marshalling error"; }
	};

	struct Durability_error: public Error {
		Durability_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Durability error"; }
	};

	struct Network_error: public Error {
		Network_error(const std::string& msg, const char* file, const int line, const char* function):
			Error(msg, file, line, function) {}
		const char* prefix() const { return "Network error"; }
	};

	struct Error_message {

		constexpr Error_message(const Error& err, const char* file, const int line, const char* function):
			_error(err), _file(file), _line(line), _function(function) {}

		friend std::ostream& operator<<(std::ostream& out, const Error_message& rhs) {
			std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			char formatted_time[25];
			std::strftime(formatted_time, sizeof(formatted_time),
				"%FT%T%z", std::localtime(&now_time));
			out << "---------- ERROR ----------"
				<< '\n' << rhs._error
				<< "\nCaught at:  " << rhs._function << '[' << rhs._file << ':' << rhs._line << ']'
				<< "\nDate:       " << formatted_time
				<< "\nBuild hash: " << REPO_VERSION
				<< "\nBuild date: " << REPO_DATE
				<< "\n---------------------------"
				<< std::endl;
			return out;
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
		constexpr K* kernel() { return _kernel; }
	private:
		K* _kernel;
	};

	enum struct Result: uint16_t {
		SUCCESS = 0,
		UNDEFINED = 1,
		ENDPOINT_NOT_CONNECTED = 3,
		NO_UPSTREAM_SERVERS_LEFT = 4,
		NO_PRINCIPAL_FOUND = 5,
		USER_ERROR = 6,
		FATAL_ERROR = 7
	};

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

		struct Auto_set_terminate_handler {
			Auto_set_terminate_handler() { std::set_terminate(error_printing_handler); }
		private:
			[[noreturn]]
			static void error_printing_handler() noexcept {
				std::exception_ptr ptr = std::current_exception();
				if (ptr) {
					try {
						std::rethrow_exception(ptr);
					} catch (Error& err) {
						std::cerr << Error_message(err, __FILE__, __LINE__, __func__) << std::endl;
					} catch (std::exception& err) {
						std::cerr << String_message(err, __FILE__, __LINE__, __func__) << std::endl;
					} catch (...) {
						std::cerr << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__) << std::endl;
					}
				} else {
					std::cerr << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__) << std::endl;
				}
				print_stack_trace();
				factory_stop(true);
				std::abort();
			}

#if defined(HAVE_BACKTRACE)
			static void print_stack_trace() noexcept {
				static const backtrace_size_t MAX_ENTRIES = 64;
				void* stack[MAX_ENTRIES];
				backtrace_size_t num_entries = ::backtrace(stack, MAX_ENTRIES);
				::backtrace_symbols_fd(stack, num_entries, STDERR_FILENO);
			}
#else
			static void print_stack_trace() {}
#endif
		} __factory_auto_set_terminate_handler;

		template<class It, class Delim>
		struct Intersperse {
			typedef typename std::remove_const<It>::type Iter;
			Intersperse(It st, It en, Delim d):
				first(st), last(en), delim(d) {}
			friend std::ostream& operator<<(std::ostream& out, const Intersperse<It,Delim>& rhs) {
				std::ostream::sentry s(out);
				if (s && rhs.first != rhs.last) {
					Iter first = rhs.first;
					Iter end = rhs.last;
					--end;
					while (first != end) {
						out << *first << rhs.delim;
						++first;
					}
					out << *end;
				}
				return out;
			}
		private:
			Iter first, last;
			Delim delim;
		};

		template<class It, class Delim>
		Intersperse<It,Delim> intersperse(It st, It en, Delim d) {
			return Intersperse<It,Delim>(st, en, d);
		}

	}

}
