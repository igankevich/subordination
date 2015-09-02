#ifndef SYSX_SECURITY_HH
#define SYSX_SECURITY_HH

#include <iostream>

#include <sysx/process.hh>
#include <sysx/network_format.hh>

namespace sysx {

	/// filter control characters from output
	template<class T>
	struct filterbuf: public std::basic_streambuf<T> {

		using typename std::basic_streambuf<T>::int_type;
		using typename std::basic_streambuf<T>::traits_type;
		using typename std::basic_streambuf<T>::char_type;

		explicit
		filterbuf(std::basic_streambuf<T>* oldbuf):
			_orig(oldbuf) {}

		~filterbuf() {
			delete this->_orig;
		}

		int_type
		overflow(int_type c = traits_type::eof()) override {
			if (isbadchar(c)) {
				return traits_type::eof();
			}
			return this->_orig->sputc(c);
		}

		std::streamsize
		xsputn(const char_type* s, std::streamsize n) override {
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

		constexpr static bool
		isbadchar(char_type c) noexcept {
			return ((c>=0 && c<=31) || (c>=128 && c<=159))
				&& c != '\r' && c != '\n';
		}

		std::basic_streambuf<T>* _orig;
	};

	/// ``Do not trust file descriptors 0, 1, 2''
	/// http://www.lst.de/~okir/blackhats/node41.html
	struct Auto_open_standard_file_descriptors {
		Auto_open_standard_file_descriptors() {
			const int MAX_ITER = 3;
			int fd = 0;
			for (int i=0; i<MAX_ITER && fd<=2; ++i) {
				fd = bits::check(::open("/dev/null", O_RDWR),
					__FILE__, __LINE__, __func__);
				if (fd > 2) {
					bits::check(::close(fd),
						__FILE__, __LINE__, __func__);
				}
			}
		}
	};

	struct Auto_filter_bad_chars_on_cout_and_cerr {
		Auto_filter_bad_chars_on_cout_and_cerr() {
			this->filter(std::cout);
			this->filter(std::cerr);
		}
	private:
		void filter(std::ostream& str) {
			std::streambuf* orig = str.rdbuf();
			str.rdbuf(new filterbuf<std::ostream::char_type>(orig));
		}
	};
	
	struct Auto_check_endiannes {
		Auto_check_endiannes() {
			union Endian {
				constexpr Endian() {}
				uint32_t i = UINT32_C(1);
				uint8_t b[4];
			} endian;
			if ((bits::is_network_byte_order() && endian.b[0] != 0)
				|| (!bits::is_network_byte_order() && endian.b[0] != 1))
			{
				throw bits::os_error("endiannes was not correctly determined at compile time",
					__FILE__, __LINE__, __func__);
			}
		}
	};

	struct Disable_sync_with_stdio {
		Disable_sync_with_stdio() {
			std::ios_base::sync_with_stdio(false);
		}
	};

}

#endif // SYSX_SECURITY_HH
