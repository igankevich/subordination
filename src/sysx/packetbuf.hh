#ifndef SYSX_PACKETBUF_HH
#define SYSX_PACKETBUF_HH

#include <iomanip>

#include <stdx/log.hh>
#include <stdx/streambuf.hh>
#include <sysx/network_format.hh>
#include <sysx/bits/buffer_category.hh>

namespace sysx {

	template<class Base>
	struct basic_packetbuf: public Base {

		typedef Base base_type;
		using base_type::gptr;
		using base_type::eback;
		using base_type::egptr;
		using base_type::setg;
		using base_type::pptr;
		using base_type::pbase;
		using base_type::epptr;
		using base_type::setp;
		using base_type::pbump;
		using base_type::xsputn;
		using typename base_type::int_type;
		using typename base_type::traits_type;
		using typename base_type::char_type;
		using typename base_type::pos_type;
		typedef uint32_t size_type;
		typedef stdx::log<basic_packetbuf> this_log;

		enum struct State {
			initial,
			header_is_ready,
			payload_is_ready
		};

		basic_packetbuf() = default;
		basic_packetbuf(basic_packetbuf&&) = default;
		basic_packetbuf(const basic_packetbuf&) = delete;
		virtual ~basic_packetbuf() = default;

		bool
		read_packet() override {
			const State old_state = _rstate;
			switch (_rstate) {
				case State::initial: this->read_kernel_packetsize(); break;
				case State::header_is_ready: this->buffer_payload(); break;
				case State::payload_is_ready: this->read_payload(); break;
			}
			if (old_state != _rstate) {
				this->dumpstate();
				this->read_packet();
			}
			return payload_is_ready();
		}

		bool
		payload_is_ready() const {
			return _rstate == State::payload_is_ready;
		}

		bool
		dirty() const {
			return pptr() != epptr();
		}

		void
		begin_packet() override {
			_packetpos = pptr() - pbase();
			put_header(0);
		}

		void
		end_packet() override {
			const pos_type end = pptr() - pbase();
			const size_type s = end - _packetpos;
			if (s == header_size()) {
				pbump(-std::streamsize(s));
			} else {
				overwrite_header(s);
			}
		}

//		template<class X> friend class basic_opacketbuf;
		template<class X> friend void append_payload(std::streambuf& buf, basic_packetbuf<X>& kbuf);

	private:

		char_type*
		packet_begin() const {
			return eback() + _ipacketpos;
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
			return _ipacketpos + pos_type(header_size());
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
				_ipacketpos = gptr() - eback();
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
				_ipacketpos = 0;
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
				<< ",start=" << _ipacketpos
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

		// output buffer
		void
		put_header(size_type s) {
			sysx::Bytes<size_type> hdr(s);
			hdr.to_network_format();
			xsputn(hdr.begin(), hdr.size());
		}

		void
		overwrite_header(size_type s) {
			sysx::Bytes<size_type> hdr(s);
			hdr.to_network_format();
			traits_type::copy(pbase() + _packetpos,
				hdr.begin(), hdr.size());
		}

		static constexpr size_type
		header_size() {
			return sizeof(_packetsize);
		}

		size_type _packetsize = 0;
		pos_type _ipacketpos = 0;
		pos_type _oldendpos = 0;
		State _rstate = State::initial;
		pos_type _packetpos = 0;
	};

	template<class X>
	void append_payload(std::streambuf& buf, basic_packetbuf<X>& rhs) {
		typedef typename basic_packetbuf<X>::size_type size_type;
		const size_type n = rhs.payloadsize();
		buf.sputn(rhs.payload_begin(), n);
		rhs.gbump(n);
	}

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


	typedef basic_kstream<char> kstream;

}

namespace stdx {

	struct temp_cat {};

	template<class Base>
	struct type_traits<sysx::basic_packetbuf<Base>> {
		static constexpr const char*
		short_name() { return "packetbuf"; }
//		typedef sysx::buffer_category category;
		typedef temp_cat category;
	};

}

#endif // SYSX_PACKETBUF_HH
