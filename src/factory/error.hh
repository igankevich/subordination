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
		TEST,
		IO,
		SHMEM
	};

	inline
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
			case Level::IO        : out << "io";      break;
			case Level::SHMEM     : out << "shm";     break;
			default               : out << "unknwn";  break;
		}
		return out;
	}

	extern Spin_mutex __logger_mutex;

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
			components::print_all_endpoints(_buf);
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
	template<> struct Logger<Level::IO       >: public No_logger {};
//	template<> struct Logger<Level::COMPONENT>: public No_logger {};
//	template<> struct Logger<Level::STRATEGY >: public No_logger {};
//	template<> struct Logger<Level::DISCOVERY>: public No_logger {};
////	template<> struct Logger<Level::GRAPH    >: public No_logger {};
//	template<> struct Logger<Level::WEBSOCKET>: public No_logger {};

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

	template<class Ret>
	Ret check(const char* func, Ret ret, Ret bad=Ret(-1)) {
		if (ret == bad) {
			throw std::system_error(std::error_code(errno, std::system_category()), func);
		}
		return ret;
	}

	template<class Ret>
	Ret check(Ret ret, const char* file, const int line, const char* func, Ret bad=Ret(-1)) {
		if (ret == bad) {
			throw Error(std::system_error(std::error_code(errno,
				std::system_category()), func),
				file, line, func);
		}
		return ret;
	}

	typedef std::decay<decltype(errno)>::type errno_type;

	template<errno_type ignored_errno>
	int check_if_not(int ret, const char* file, const int line, const char* func, int bad=-1) {
		return errno == ignored_errno ? 0 : check<int>(ret, file, line, func, bad);
	}

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
				<< "\nProcess ID: " << this_process::id()
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
		K* kernel() { return _kernel; }
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


		template <class T, class Ch=char, class Tr=std::char_traits<Ch>>
		class intersperse_iterator:
			public std::iterator<std::output_iterator_tag, void, void, void, void>
		{
			typedef intersperse_iterator<T,Ch,Tr> This;
			std::basic_ostream<Ch,Tr>* ostr;
			const Ch* delim;
			bool first = true;
		
		public:
			typedef Ch char_type;
			typedef Tr traits_type;
			typedef std::basic_ostream<Ch,Tr> ostream_type;

			intersperse_iterator(ostream_type& s, const Ch* delimiter=0):
				ostr(&s), delim(delimiter) {}
			intersperse_iterator(const intersperse_iterator<T,Ch,Tr>& x) = default;
			~intersperse_iterator() = default;
		
			This& operator=(const T& value) {
				if (delim != 0 && !first) *ostr << delim;
				*ostr << value;
				if (first) { first = false; }
				return *this;
			}
		
			This& operator*() { return *this; }
			This& operator++() { return *this; }
			This& operator++(int) { return *this; }
		};

		struct debug_stream {
			explicit debug_stream(const std::ios& s): str(s) {}
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


		template<class M>
		struct Print_values {
			typedef M map_type;
			typedef typename map_type::key_type key_type;
			typedef typename map_type::value_type value_type;
			Print_values(const M& m): map(m) {}
			friend std::ostream& operator<<(std::ostream& out, const Print_values& rhs) {
				out << '{';
				typedef decltype(*rhs.map.begin()->second) ret_type;
				intersperse_iterator<ret_type> it(out, ",");
				std::transform(rhs.map.begin(), rhs.map.end(), it,
					[] (const value_type& pair) -> const ret_type& {
						return *pair.second;
					}
				);
				out << '}';
				return out;
			}
		private:
			const M& map;
		};

		template<class M>
		Print_values<M> print_values(const M& m) { return Print_values<M>(m); }

		template<class It1, class It2>
		struct paired_iterator: public std::iterator<
			typename std::iterator_traits<It1>::iterator_category,
			std::tuple<typename std::iterator_traits<It1>::value_type,typename std::iterator_traits<It2>::value_type>,
			std::ptrdiff_t,
			std::tuple<typename std::iterator_traits<It1>::value_type*,typename std::iterator_traits<It2>::value_type*>,
			std::tuple<typename std::iterator_traits<It1>::value_type&&,typename std::iterator_traits<It2>::value_type&&>
			>
		{
			typedef paired_iterator<It1,It2> this_type;
			typedef typename std::iterator_traits<this_type>::reference reference;
			typedef typename std::iterator_traits<this_type>::pointer pointer;
			typedef typename std::iterator_traits<this_type>::difference_type difference_type;
			typedef const reference const_reference;
			typedef const pointer const_pointer;

			typedef typename std::iterator_traits<It1>::value_type&& reference1;
			typedef typename std::iterator_traits<It2>::value_type&& reference2;
			typedef const reference1 const_reference1;
			typedef const reference2 const_reference2;

			constexpr paired_iterator(It1 x, It2 y): iter1(x), iter2(y) {}
			paired_iterator(const paired_iterator& rhs) = default;

			constexpr bool operator==(const this_type& rhs) const { return iter1 == rhs.iter1; }
			constexpr bool operator!=(const this_type& rhs) const { return !this->operator==(rhs); }

			const_reference operator*() const { return std::tuple<const_reference1,const_reference2>(*iter1,*iter2); }
			reference operator*() { return std::tuple<reference1,reference2>(std::move(*iter1),std::move(*iter2)); }
			const_pointer operator->() const { return std::make_tuple(iter1.operator->(),iter2.operator->()); }
			pointer operator->() { return std::make_tuple(iter1.operator->(),iter2.operator->()); }
			this_type& operator++() { ++iter1; ++iter2; return *this; }
			this_type operator++(int) { this_type tmp(*this); ++iter1; ++iter2; return tmp; }
			difference_type operator-(const this_type& rhs) const {
				return std::distance(rhs.iter1, iter1);
			}
			this_type operator+(difference_type rhs) const {
				return this_type(std::advance(iter1, rhs), std::advance(iter2, rhs));
			}
			It1 first() { return iter1; }
			It2 second() { return iter2; }

		private:
			It1 iter1;
			It2 iter2;
		};

		template<class It1, class It2>
		paired_iterator<It1,It2> make_paired(It1 it1, It2 it2) {
			return paired_iterator<It1,It2>(it1, it2);
		}

		template<size_t No, class F>
		struct Apply_to {
			constexpr explicit Apply_to(F&& f): func(f) {}
			template<class Arg>
			auto operator()(const Arg& rhs) ->
				typename std::result_of<F&(decltype(std::get<No>(rhs)))>::type
			{
				return func(std::get<No>(rhs));
			}
		private:
			F&& func;
		};

		template<size_t No, class F>
		constexpr Apply_to<No,F> apply_to(F&& f) {
			return Apply_to<No,F>(f);
		}
		
	}

}
