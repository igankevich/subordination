#ifndef FACTORY_EXT_KERNELBUF_HH
#define FACTORY_EXT_KERNELBUF_HH

#include <iomanip>
#include <sysx/network_format.hh>
#include <factory/ext/intro.hh>
#include <stdx/streambuf.hh>

namespace factory {

	namespace components {

		template<class Base>
		struct basic_ikernelbuf: public Base {

			typedef Base base_type;
			using base_type::gptr;
			using base_type::eback;
			using base_type::egptr;
			using base_type::setg;
			using typename base_type::int_type;
			using typename base_type::traits_type;
			using typename base_type::char_type;
			using typename base_type::pos_type;
			typedef uint32_t size_type;
			typedef stdx::log<basic_ikernelbuf> this_log;

			enum struct State {
				initial,
				header_is_ready,
				payload_is_ready
			};

			basic_ikernelbuf() = default;
			basic_ikernelbuf(basic_ikernelbuf&&) = default;
			basic_ikernelbuf(const basic_ikernelbuf&) = delete;
			virtual ~basic_ikernelbuf() = default;

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for ibasic_kernelbuf");

//			int_type
//			underflow() override {
//				assert(gptr() == egptr());
//				int_type ret = Base::underflow();
//				this->update_state();
//				if (this->_rstate == State::payload_is_ready && egptr() != gptr()) {
//					return *gptr();
//				}
//				return traits_type::eof();
//			}
//
//			std::streamsize
//			xsgetn(char_type* s, std::streamsize n) override {
//				if (egptr() == gptr()) {
//					Base::underflow();
//				}
//				this->update_state();
//				if (egptr() == gptr() || _rstate != State::payload_is_ready) {
//					return std::streamsize(0);
//				}
//				return Base::xsgetn(s, n);
//			}

//			void
//			pubunderflow() {
//				const pos_type old_offset = gptr() - eback();
//				setg(eback(), egptr(), egptr());
//				Base::underflow();
//				setg(eback(), eback() + old_offset, egptr());
//			}

			bool
			update_state() {
				const State old_state = _rstate;
				switch (_rstate) {
					case State::initial: this->read_kernel_packetsize(); break;
					case State::header_is_ready: this->buffer_payload(); break;
					case State::payload_is_ready: this->read_payload(); break;
				}
				if (old_state != _rstate) {
					this->dumpstate();
					this->update_state();
				}
				return payload_is_ready();
			}

			bool
			payload_is_ready() const {
				return _rstate == State::payload_is_ready;
			}

//			template<class X> friend class basic_okernelbuf;
			template<class X> friend void append_payload(std::streambuf& buf, basic_ikernelbuf<X>& kbuf);

		private:

			char_type*
			packet_begin() const {
				return eback() + _packetpos;
			}

			char_type*
			packet_end() const {
				return packet_begin() + _packetsize;
			}

			char_type*
			payload_begin() const {
				return packet_begin() + hdrsize();
			}

			char_type*
			payload_end() const {
				return payload_begin() + payloadsize();
			}

			pos_type packetpos() const { return this->_packetpos; }
			pos_type payloadpos() const { return this->_packetpos + static_cast<pos_type>(this->hdrsize()); }
			size_type packetsize() const { return this->_packetsize; }
			size_type bytes_left_until_the_end_of_the_packet() const {
				return this->_packetsize - (gptr() - (eback() + this->payloadpos()));
			}

			void read_kernel_packetsize() {
				size_type count = egptr() - gptr();
				if (!(count < this->hdrsize())) {
					sysx::Bytes<size_type> size(gptr(), gptr() + this->hdrsize());
					size.to_host_format();
					_packetpos = gptr() - eback();
					_packetsize = size;
					this->gbump(this->hdrsize());
					this->dumpstate();
					this->sets(State::header_is_ready);
				}
			}

			void buffer_payload() {
				pos_type endpos = egptr() - eback();
				if (this->_oldendpos < endpos) {
					this->_oldendpos = endpos;
				}
				if (egptr() >= packet_end()) {
					this->setg(eback(), payload_begin(), payload_end());
					this->sets(State::payload_is_ready);
				} else {
					// TODO: remove if try_to_buffer_payload() is used
					this->setg(eback(), egptr(), egptr());
				}
			}

			void read_payload() {
				if (gptr() == payload_end()) {
					pos_type endpos = egptr() - eback();
					if (this->_oldendpos > endpos) {
						this->setg(eback(), gptr(), eback() + this->_oldendpos);
					}
					_packetpos = 0;
					_packetsize = 0;
					_oldendpos = 0;
					this->sets(State::initial);
				}
			}

			void dumpstate() {
				this_log() << std::setw(20) << std::left << _rstate
					<< "pptr=" << this->pptr() - this->pbase()
					<< ",epptr=" << this->epptr() - this->pbase()
					<< ",gptr=" << gptr() - eback()
					<< ",egptr=" << egptr() - eback()
					<< ",size=" << this->packetsize()
					<< ",start=" << this->packetpos()
					<< ",oldpos=" << this->_oldendpos
					<< std::endl;
			}

			friend std::ostream&
			operator<<(std::ostream& out, State rhs) {
				switch (rhs) {
					case State::initial: out << "initial"; break;
					case State::header_is_ready: out << "header_is_ready"; break;
					case State::payload_is_ready: out << "payload_is_ready"; break;
					default: break;
				}
				return out;
			}

			void
			sets(State rhs) {
				this_log() << "oldstate=" << this->_rstate
					<< ",newstate=" << rhs << std::endl;
				this->_rstate = rhs;
			}

			size_type
			payloadsize() const {
				return this->_packetsize - this->hdrsize();
			}

			static constexpr size_type
			hdrsize() {
				return sizeof(_packetsize);
			}

			size_type _packetsize = 0;
			pos_type _packetpos = 0;
			pos_type _oldendpos = 0;
			State _rstate = State::initial;
		};

		template<class X>
		void append_payload(std::streambuf& buf, basic_ikernelbuf<X>& rhs) {
			typedef typename basic_ikernelbuf<X>::size_type size_type;
			const size_type n = rhs.bytes_left_until_the_end_of_the_packet();
			buf.sputn(rhs.gptr(), n);
			rhs.gbump(n);
		}

		template<class Base>
		struct basic_okernelbuf: public Base {

			typedef Base base_type;
			using base_type::pptr;
			using base_type::pbase;
			using base_type::epptr;
			using base_type::setp;
			using base_type::gptr;
			using base_type::eback;
			using base_type::egptr;
			using base_type::pbump;
			using typename base_type::int_type;
			using typename base_type::traits_type;
			using typename base_type::char_type;
			using typename base_type::pos_type;
			typedef uint32_t size_type;
			typedef stdx::log<basic_okernelbuf> this_log;

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for basic_okernelbuf");

			basic_okernelbuf() = default;
			basic_okernelbuf(const basic_okernelbuf&) = delete;
			basic_okernelbuf(basic_okernelbuf&&) = default;
			virtual ~basic_okernelbuf() { this->end_packet(); }

			bool
			dirty() const {
				return pptr() != epptr();
			}

			void
			begin_packet() {
				_packetpos = pptr() - pbase();
				putsize(0);
				this_log() << "begin_packet()     "
					<< "pbase=" << (void*)this->pbase()
					<< ", pptr=" << (void*)this->pptr()
					<< ", eback=" << (void*)eback()
					<< ", gptr=" << (void*)gptr()
					<< ", egptr=" << (void*)egptr()
					<< std::endl;
			}

			void
			end_packet() {
				const pos_type end = pptr() - pbase();
				const size_type s = end - _packetpos;
				if (s == header_size()) {
					pbump(-std::streamsize(s));
				} else {
					overwrite_size(s);
				}
				this_log() << "end_packet(): size=" << s << std::endl;
			}

		private:

			void
			putsize(size_type s) {
				sysx::Bytes<size_type> pckt_size(s);
				pckt_size.to_network_format();
				Base::xsputn(pckt_size.begin(), pckt_size.size());
			}

			void
			overwrite_size(size_type s) {
				sysx::Bytes<size_type> pckt_size(s);
				pckt_size.to_network_format();
				traits_type::copy(pbase() + _packetpos,
					pckt_size.begin(), pckt_size.size());
			}

			static constexpr std::streamsize
			header_size() {
				return sizeof(size_type);
			}

			pos_type _packetpos = 0;
		};

		template<class Base1, class Base2=Base1>
		struct basic_kernelbuf: public basic_okernelbuf<basic_ikernelbuf<Base1>> {};

		template<class Base>
		struct basic_kstream: public std::basic_iostream<typename Base::char_type, typename Base::traits_type> {
			typedef basic_kernelbuf<Base> kernelbuf_type;
			typedef typename Base::char_type Ch;
			typedef typename Base::traits_type Tr;
			typedef std::basic_iostream<Ch,Tr> iostream_type;
			basic_kstream(): iostream_type(nullptr), _kernelbuf()
				{ this->init(&this->_kernelbuf); }
		private:
			kernelbuf_type _kernelbuf;
		};


//		struct end_packet {
//			friend std::ostream&
//			operator<<(std::ostream& out, end_packet) {
//				out.rdbuf()->pubsync();
//				return out;
//			}
//		};

	//	struct underflow {
	//		friend std::istream&
	//		operator>>(std::istream& in, underflow) {
	//			// TODO: loop until source is exhausted
	//			std::istream::pos_type old_pos = in.rdbuf()->pubseekoff(0, std::ios_base::cur, std::ios_base::in);
	//			in.rdbuf()->pubseekoff(0, std::ios_base::end, std::ios_base::in);
	//			in.rdbuf()->sgetc(); // underflows the stream buffer
	//			in.rdbuf()->pubseekpos(old_pos);
	//			return in;
	//		}
	//	};

	}

	typedef components::basic_kstream<char> kstream;

}

namespace stdx {

	struct temp_cat {};

	template<class Base>
	struct type_traits<factory::components::basic_ikernelbuf<Base>> {
		static constexpr const char*
		short_name() { return "ikernelbuf"; }
//		typedef factory::components::buffer_category category;
		typedef temp_cat category;
	};

	template<class Base>
	struct type_traits<factory::components::basic_okernelbuf<Base>> {
		static constexpr const char*
		short_name() { return "okernelbuf"; }
		typedef factory::components::buffer_category category;
	};

}

#endif // FACTORY_EXT_KERNELBUF_HH
