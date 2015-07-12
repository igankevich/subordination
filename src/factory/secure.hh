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
	}
}
