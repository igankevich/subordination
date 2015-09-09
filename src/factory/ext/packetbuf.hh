#ifndef FACTORY_EXT_PACKETBUF_HH
#define FACTORY_EXT_PACKETBUF_HH

#include <iomanip>
#include <sysx/network_format.hh>
#include <factory/ext/intro.hh>
#include <stdx/streambuf.hh>

namespace factory {

	namespace components {

		template<class Base>
		struct basic_ipacketbuf: public Base {

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
			typedef stdx::log<basic_ipacketbuf> this_log;

			enum struct State {
				initial,
				header_is_ready,
				payload_is_ready
			};

			basic_ipacketbuf() = default;
			basic_ipacketbuf(basic_ipacketbuf&&) = default;
			basic_ipacketbuf(const basic_ipacketbuf&) = delete;
			virtual ~basic_ipacketbuf() = default;

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for ibasic_packetbuf");

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

//			template<class X> friend class basic_opacketbuf;
			template<class X> friend void append_payload(std::streambuf& buf, basic_ipacketbuf<X>& kbuf);

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
				return packet_begin() + header_size();
			}

			char_type*
			payload_end() const {
				return payload_begin() + payloadsize();
			}

			pos_type
			payloadpos() const {
				return _packetpos + pos_type(header_size());
			}

			size_type
			bytes_left_until_the_end_of_the_packet() const {
				return _packetsize - (gptr() - (eback() + payloadpos()));
			}

			void read_kernel_packetsize() {
				size_type count = egptr() - gptr();
				if (!(count < this->header_size())) {
					sysx::Bytes<size_type> size(gptr(), gptr() + this->header_size());
					size.to_host_format();
					_packetpos = gptr() - eback();
					_packetsize = size;
					this->gbump(this->header_size());
					this->dumpstate();
					this->sets(State::header_is_ready);
				}
			}

			void buffer_payload() {
				pos_type endpos = egptr() - eback();
				if (_oldendpos < endpos) {
					_oldendpos = endpos;
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
					if (_oldendpos > endpos) {
						this->setg(eback(), gptr(), eback() + _oldendpos);
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
					<< ",size=" << _packetsize
					<< ",start=" << _packetpos
					<< ",oldpos=" << _oldendpos
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
				this_log() << "oldstate=" << _rstate
					<< ",newstate=" << rhs << std::endl;
				_rstate = rhs;
			}

			size_type
			payloadsize() const {
				return _packetsize - header_size();
			}

			static constexpr size_type
			header_size() {
				return sizeof(_packetsize);
			}

			size_type _packetsize = 0;
			pos_type _packetpos = 0;
			pos_type _oldendpos = 0;
			State _rstate = State::initial;
		};

		template<class X>
		void append_payload(std::streambuf& buf, basic_ipacketbuf<X>& rhs) {
			typedef typename basic_ipacketbuf<X>::size_type size_type;
			const size_type n = rhs.bytes_left_until_the_end_of_the_packet();
			buf.sputn(rhs.gptr(), n);
			rhs.gbump(n);
		}

		template<class Base>
		struct basic_opacketbuf: public Base {

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
			typedef stdx::log<basic_opacketbuf> this_log;

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for basic_opacketbuf");

			basic_opacketbuf() = default;
			basic_opacketbuf(const basic_opacketbuf&) = delete;
			basic_opacketbuf(basic_opacketbuf&&) = default;
			virtual ~basic_opacketbuf() { this->end_packet(); }

			bool
			dirty() const {
				return pptr() != epptr();
			}

			void
			begin_packet() {
				_packetpos = pptr() - pbase();
				put_header(0);
			}

			void
			end_packet() {
				const pos_type end = pptr() - pbase();
				const size_type s = end - _packetpos;
				if (s == header_size()) {
					pbump(-std::streamsize(s));
				} else {
					overwrite_header(s);
				}
			}

		private:

			void
			put_header(size_type s) {
				sysx::Bytes<size_type> hdr(s);
				hdr.to_network_format();
				Base::xsputn(hdr.begin(), hdr.size());
			}

			void
			overwrite_header(size_type s) {
				sysx::Bytes<size_type> hdr(s);
				hdr.to_network_format();
				traits_type::copy(pbase() + _packetpos,
					hdr.begin(), hdr.size());
			}

			static constexpr std::streamsize
			header_size() {
				return sizeof(size_type);
			}

			pos_type _packetpos = 0;
		};

		template<class Base1, class Base2=Base1>
		struct basic_packetbuf: public basic_opacketbuf<basic_ipacketbuf<Base1>> {};

		template<class Base>
		struct basic_kstream: public std::basic_iostream<typename Base::char_type, typename Base::traits_type> {
			typedef basic_packetbuf<Base> packetbuf_type;
			typedef typename Base::char_type Ch;
			typedef typename Base::traits_type Tr;
			typedef std::basic_iostream<Ch,Tr> iostream_type;
			basic_kstream(): iostream_type(nullptr), _packetbuf()
				{ this->init(&_packetbuf); }

			void
			begin_packet() {
				_packetbuf.begin_packet();
			}

			void
			end_packet() {
				_packetbuf.end_packet();
			}

			bool
			update_state() {
				return _packetbuf.update_state();
			}

		private:
			packetbuf_type _packetbuf;
		};

	}

	typedef components::basic_kstream<char> kstream;

}

namespace stdx {

	struct temp_cat {};

	template<class Base>
	struct type_traits<factory::components::basic_ipacketbuf<Base>> {
		static constexpr const char*
		short_name() { return "ipacketbuf"; }
//		typedef factory::components::buffer_category category;
		typedef temp_cat category;
	};

	template<class Base>
	struct type_traits<factory::components::basic_opacketbuf<Base>> {
		static constexpr const char*
		short_name() { return "opacketbuf"; }
		typedef factory::components::buffer_category category;
	};

}

#endif // FACTORY_EXT_PACKETBUF_HH
