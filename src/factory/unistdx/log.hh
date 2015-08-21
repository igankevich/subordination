#include <syslog.h>

namespace factory {

	namespace unix {

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_logbuf: public std::basic_streambuf<Ch,Tr> {

			using typename std::basic_streambuf<Ch,Tr>::int_type;
			using typename std::basic_streambuf<Ch,Tr>::traits_type;
			using typename std::basic_streambuf<Ch,Tr>::char_type;
			using typename std::basic_streambuf<Ch,Tr>::pos_type;
			using typename std::basic_streambuf<Ch,Tr>::off_type;
			typedef std::recursive_mutex mutex_type;
			typedef std::lock_guard<mutex_type> lock_type;
			typedef std::basic_string<Ch,Tr> buf_type;
			typedef std::basic_ostream<Ch,Tr>& stream_type;
//			typedef typename buf_type::iterator buf_iterator;

			enum struct state_type {
				begin_message,
				write_message
			};

			struct thread_buf {
				buf_type buf{};
				state_type state = state_type::begin_message;
			};

			explicit
			basic_logbuf(std::basic_ostream<Ch,Tr>& s):
			_stream(s), _bufs()
			{}

			int_type
			overflow(int_type c = traits_type::eof()) override {
				lock_type lock(_mutex);
				begin_message();
				this_thread_buf().push_back(c);
				return c;
			}

			std::streamsize
			xsputn(const char_type* s, std::streamsize n) override {
				lock_type lock(_mutex);
				begin_message();
				this_thread_buf().append(s, n);
				return n;
			}

			int
			sync() override {
				lock_type lock(_mutex);
				return end_message() ? 0 : -1;
			}

		private:

			void
			begin_message() {
				if (this_state() == state_type::begin_message) {
					this_state() = state_type::write_message;
					write_header();
				}
			}

			bool
			end_message() {
				bool ret = false;
				if (this_state() == state_type::write_message) {
					write_log();
					this_state() = state_type::begin_message;
					ret = true;
				}
				return ret;
			}

			void
			write_header() {
////				components::print_all_endpoints(_stream);
////				_stream << SEP << std::this_thread::get_id() << SEP;
//				buf_type& buf = this_thread_buf();
//				typedef decltype(std::back_inserter(buf)) buf_iterator;
//				buf.push_back(SEP);
//				bits::Bytes<size_t> tmp(_hash(std::this_thread::get_id()));
//				buf.append(tmp.begin(), tmp.size());
////				std::use_facet<std::num_put<size_t,buf_iterator>>(_stream.getloc()).
////					put(std::back_inserter(buf), _stream, ' ',
////					_hash(std::this_thread::get_id()));
//				buf.push_back(SEP);
			}

			void
			write_log() {
				buf_type& buf = this_thread_buf();
				if (!buf.empty()) {
					write_syslog(buf.c_str());
					buf.clear();
				}
			}

			void
			write_syslog(const char_type* message) {
				::syslog(LOG_INFO, "%s", message);
			}

			buf_type&
			this_thread_buf() {
				return _bufs[std::this_thread::get_id()].buf;
			}

			state_type&
			this_state() {
				return _bufs[std::this_thread::get_id()].state;
			}

			mutex_type _mutex;
			stream_type& _stream;
			std::unordered_map<std::thread::id, thread_buf> _bufs;
			std::hash<std::thread::id> _hash;

//			constexpr static const char SEP = ' ';
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
