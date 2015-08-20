#include <syslog.h>

namespace factory {

	namespace unix {

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_logbuf: public std::basic_stringbuf<Ch,Tr> {

			using typename std::basic_stringbuf<Ch,Tr>::int_type;
			using typename std::basic_stringbuf<Ch,Tr>::traits_type;
			using typename std::basic_stringbuf<Ch,Tr>::char_type;
			using typename std::basic_stringbuf<Ch,Tr>::pos_type;
			using typename std::basic_stringbuf<Ch,Tr>::off_type;
			typedef std::mutex mutex_type;
			typedef std::unique_lock<mutex_type> lock_type;
			typedef std::basic_stringbuf<Ch,Tr> base_type;

			enum struct state_type {
				begin_message,
				write_message
			};

			explicit
			basic_logbuf(std::basic_ostream<Ch,Tr>& s):
				_stream(s) {}

			int_type
			overflow(int_type c = traits_type::eof()) override {
				begin_message();
				return base_type::overflow(c);
			}

			std::streamsize
			xsputn(const char_type* s, std::streamsize n) override {
				begin_message();
				return base_type::xsputn(s, n);
			}

			int
			sync() override {
				return end_message() ? 0 : -1;
			}

		private:

			void
			begin_message() {
				if (_state == state_type::begin_message) {
					_mutex.lock();
					_state = state_type::write_message;
					write_header();
				}
			}

			bool
			end_message() {
				if (_state == state_type::write_message) {
					write_log();
					_mutex.unlock();
					_state = state_type::begin_message;
				}
				return _state == state_type::begin_message;
			}

			void
			write_header() {
				components::print_all_endpoints(_stream);
				_stream << SEP << std::this_thread::get_id() << SEP;
			}

			void
			write_log() {
				std::streamsize n = this->pptr() - this->pbase();
				if (n > 0) {
					this->sputc(char_type());
					write_log(this->pbase());
					this->pbump(-n-1);
				}
			}

			void
			write_log(char_type* message) {
				std::cout << "Log message: " << message << std::endl;
				::syslog(LOG_INFO, "%s", message);
			}

			mutex_type _mutex;
			state_type _state = state_type::begin_message;
			std::basic_ostream<Ch,Tr>& _stream;

			constexpr static const char SEP = ' ';
		};

		struct Install_syslog {

			explicit
			Install_syslog(std::ostream& s):
			str(s), newbuf(s), oldbuf(str.rdbuf(&newbuf))
			{}

			~Install_syslog() {
				str.rdbuf(oldbuf);
			}

		private:
			std::ostream& str;
			basic_logbuf<char> newbuf;
			std::streambuf* oldbuf = nullptr;
		};

		Install_syslog __syslog1(std::clog);

	}

}
