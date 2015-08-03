namespace factory {
	namespace components {

		/// ``Do not trust file descriptors 0, 1, 2''
		/// http://www.lst.de/~okir/blackhats/node41.html
		struct Auto_open_standard_file_descriptors {
			Auto_open_standard_file_descriptors() {
				const int MAX_ITER = 3;
				int fd = 0;
				for (int i=0; i<MAX_ITER && fd<=2; ++i) {
					fd = check("open()", ::open("/dev/null", O_RDWR));
					if (fd > 2) {
						check("close()", ::close(fd));
					}
				}
			}
		} __auto_open_standard_file_descriptors;

		// filter control characters from output
		template<class T>
		struct Filter_buf: public std::basic_streambuf<T> {
			using typename std::basic_streambuf<T>::int_type;
			using typename std::basic_streambuf<T>::traits_type;
			using typename std::basic_streambuf<T>::char_type;
			explicit Filter_buf(std::basic_streambuf<T>* oldbuf):
				_orig(oldbuf) {}
			~Filter_buf() {
				delete this->_orig;
			}
			int_type overflow(int_type c = traits_type::eof()) {
				if (isbadchar(c)) {
					return traits_type::eof();
				}
				return this->_orig->sputc(c);
			}
			std::streamsize xsputn(const char_type* s, std::streamsize n) {
				char_type* first = const_cast<char_type*>(s);
				char_type* last = first + n;
				while (first != last) {
					if (!isbadchar(*first)) {
						if (this->_orig->sputc(*first) == traits_type::eof()) {
							break;
						}
					}
					++first;
				}
				return static_cast<std::streamsize>(first - s);
			}

		private:
			constexpr static
			bool isbadchar(char_type c) {
				return ((c>=0 && c<=31) || (c>=128 && c<=159))
					&& c != '\r' && c != '\n';
			}
			std::basic_streambuf<T>* _orig;
		};

		struct Auto_filter_bad_chars_on_cout_and_cerr {
			Auto_filter_bad_chars_on_cout_and_cerr() {
				this->filter(std::cout);
				this->filter(std::cerr);
			}
		private:
			void filter(std::ostream& str) {
				std::streambuf* orig = str.rdbuf();
				str.rdbuf(new Filter_buf<std::ostream::char_type>(orig));
			}
		} __auto_filter_bad_chars_on_cout_and_cerr;

		struct Auto_check_endiannes {
			Auto_check_endiannes() {
				union Endian {
					constexpr Endian() {}
					uint32_t i = UINT32_C(1);
					uint8_t b[4];
				} endian;
				if ((is_network_byte_order() && endian.b[0] != 0)
					|| (!is_network_byte_order() && endian.b[0] != 1))
				{
					throw Error("endiannes was not correctly determined at compile time",
						__FILE__, __LINE__, __func__);
				}
			}
		} __factory_auto_check_endiannes;

		struct Auto_set_terminate_handler {
			Auto_set_terminate_handler() { std::set_terminate(error_printing_handler); }
		private:
			static void error_printing_handler() noexcept {
				static volatile bool called = false;
				if (called) { return; }
				called = true;
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
					std::cerr << String_message("terminate called without an active exception",
						__FILE__, __LINE__, __func__) << std::endl;
				}
				print_stack_trace();
				stop_all_factories(true);
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

	}
}
