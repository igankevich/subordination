namespace factory {

	enum struct Level {
		KERNEL = 0,
		SERVER = 1,
		HANDLER = 2,
		COMPONENT = 3,
		STRATEGY = 3
	};

	struct Logger {

		Logger(Level l = Level::KERNEL) {
			_buf << std::setw(int(l)) << ' ';
		}
	
		template<class T>
		Logger& operator<<(const T& rhs) {
			_buf << rhs;
			return *this;
		}
	
		Logger& operator<<(std::ostream& ( *pf )(std::ostream&)) {
			_buf << pf;
			if (pf == (std::ostream& (*)(std::ostream&))&std::endl) {
				std::clog << _buf.str() << std::flush;
				_buf.clear();
			}
			return *this;
		}

		std::ostream& ostream() { return _buf; }
	
	private:
		std::stringstream _buf;
	};

	std::string log_filename() {
		std::stringstream s;
		s << "/tmp/" << ::getpid() << ".log";
		return s.str();
	}

	int check(const char* func, int ret) {
		if (ret < 0) {
			throw std::system_error(std::error_code(errno, std::system_category()), func);
		}
		return ret;
	}

	const char* check_inet(const char* func, const char* ret) {
		if (ret == 0) {
			throw std::system_error(std::error_code(errno, std::system_category()), func);
		}
		return ret;
	}

	struct Error: public std::runtime_error {

		Error(const std::string& msg, const char* file, const int line, const char* function):
			std::runtime_error(msg), _file(file), _line(line), _function(function)
		{}

		virtual const char* prefix() const { return "Error"; }

		friend std::ostream& operator<<(std::ostream& out, const Error& rhs) {
			out << rhs.prefix() << ". " << rhs.what() << "\n\t";
			out << rhs._function << '[' << rhs._file << ':' << rhs._line << ']' << '\n';
			return out;
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

		Error_message(const Error& err, const char* file, const int line, const char* function):
			_error(err), _file(file), _line(line), _function(function) {}

		friend std::ostream& operator<<(std::ostream& out, const Error_message& rhs) {
			std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			char formatted_time[20];
			std::strftime(formatted_time, 20, "%F %T", std::localtime(&now_time));
			out << formatted_time << ' ' << rhs._error << '\t'
				<< rhs._function << '[' << rhs._file << ':' << rhs._line << ']'
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

		String_message(const char* err, const char* file, const int line, const char* function):
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

	const char* UNKNOWN_ERROR = "Unknown error.";

	template<class K>
	struct No_principal_found {
		explicit No_principal_found(K* k): _kernel(k) {}
		K* kernel() { return _kernel; }
	private:
		K* _kernel;
	};

	enum struct Result: uint16_t {
		SUCCESS = 0,
		UNDEFINED = 1,
		UNDEFINED_DOWNSTREAM = 2,
		ENDPOINT_NOT_CONNECTED = 3,
		NO_UPSTREAM_SERVERS_LEFT = 4,
		NO_PRINCIPAL_FOUND = 5
	};

}
