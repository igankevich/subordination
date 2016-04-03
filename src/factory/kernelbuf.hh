#ifndef FACTORY_KERNELBUF_HH
#define FACTORY_KERNELBUF_HH

#include <iomanip>
#include <type_traits>

#include <stdx/log.hh>
#include <stdx/packetbuf.hh>
#include <sysx/network_format.hh>
#include <sysx/bits/buffer_category.hh>

namespace factory {

	template<class Base>
	struct basic_kernelbuf: public Base {

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
		using base_type::setpacket;
		// packetbuf protected interface
		using base_type::payload_begin;
		using base_type::payload_end;
		using base_type::payloadpos;
		using base_type::payloadsize;
		using base_type::packet_begin;
		using base_type::packet_end;
		using base_type::packetpos;
		using base_type::packetsize;
		using typename base_type::int_type;
		using typename base_type::traits_type;
		using typename base_type::char_type;
		using typename base_type::pos_type;
		typedef uint32_t size_type;
		typedef stdx::log<basic_kernelbuf> this_log;

		typedef stdx::basic_packetbuf<char_type,traits_type> good_base_type;
		static_assert(std::is_base_of<good_base_type, Base>::value,
			"bad base class");

		enum struct State {
			initial,
			header_is_ready,
			payload_is_ready
		};

		basic_kernelbuf() = default;
		basic_kernelbuf(basic_kernelbuf&&) = default;
		basic_kernelbuf(const basic_kernelbuf&) = delete;
		virtual ~basic_kernelbuf() = default;

		template<class ... Args>
		basic_kernelbuf(Args&& ... args):
		Base(std::forward<Args>(args)...) {}

		int
		sync() override {
			int ret = Base::sync();
			this->debug_state(__func__);
			return ret;
		}

		bool
		read_packet() override {
			State old_state;
			do {
				old_state = _rstate;
				switch (_rstate) {
					case State::initial: this->read_kernel_packetsize(); break;
					case State::header_is_ready: this->buffer_payload(); break;
					case State::payload_is_ready: this->read_payload(); break;
				}
			} while (old_state != _rstate);
			return payload_is_ready();
		}

		void
		skip_packet() override {
			if (_rstate == State::payload_is_ready) {
				reset_packet();
//				this->setg(eback(), payload_end(), egptr());
			}
		}

		bool
		payload_is_ready() const {
			return _rstate == State::payload_is_ready;
		}

		bool
		dirty() const {
			return pptr() != pbase();
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

	private:
		typedef sysx::Bytes<size_type, char_type> bytes_type;

		void read_kernel_packetsize() {
			const size_type count = egptr() - gptr();
			if (!(count < this->header_size())) {
				bytes_type size(gptr(), gptr() + this->header_size());
				size.to_host_format();
				if (size.value() >= header_size()) {
					const pos_type p = gptr() - eback();
					setpacket(p, p + pos_type(header_size()), size);
					this->gbump(this->header_size());
					this->sets(State::header_is_ready, __func__);
				} else {
					this->gbump(this->header_size());
				}
			}
		}

		void buffer_payload() {
			const pos_type endpos = egptr() - eback();
			if (_oldendpos < endpos) {
				_oldendpos = endpos;
			}
			if (egptr() >= packet_end()) {
				this->setg(eback(), payload_begin(), payload_end());
				this->sets(State::payload_is_ready, __func__);
			}
		}

		void read_payload() {
			if (gptr() == payload_end()) {
				reset_packet();
			}
		}

		void
		reset_packet() {
			const pos_type endpos = egptr() - eback();
			if (_oldendpos > endpos) {
				this->setg(eback(), payload_end(), eback() + _oldendpos);
			}
			setpacket(0, 0, 0);
			_oldendpos = 0;
			this->sets(State::initial, __func__);
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
		debug_state(const char* where=nullptr) {
			sets(_rstate, where);
		}

		void
		sets(State rhs, const char* where=nullptr) {
			#ifndef NDEBUG
			std::stringstream tmp;
			tmp << _rstate << "->" << rhs;
			this_log log;
			if (where) {
				log << std::setw(40) << std::left << where;
			}
			log << std::setw(40) << tmp.str()
				<< "put={" << pptr() - pbase() << ',' << epptr() - pbase() << '}'
				<< ",get={" << gptr() - eback() << ',' << egptr() - eback() << '}'
				<< ",packet={"
				<< packet_begin() - eback() << ','
				<< payload_begin() - eback() << ','
				<< packet_end() - eback()
				<< '}'
				<< ",save_egptr=" << _oldendpos
				<< std::endl;
			#endif
			_rstate = rhs;
		}

		// output buffer
		void
		put_header(size_type s) {
			bytes_type hdr(s);
			hdr.to_network_format();
			xsputn(hdr.begin(), hdr.size());
		}

		void
		overwrite_header(size_type s) {
			bytes_type hdr(s);
			hdr.to_network_format();
			traits_type::copy(pbase() + _packetpos,
				hdr.begin(), hdr.size());
		}

		static constexpr size_type
		header_size() {
			return sizeof(size_type);
		}

		pos_type _oldendpos = 0;
		State _rstate = State::initial;
		pos_type _packetpos = 0;
	};

	//template<class Base>
	//struct basic_kstream: public std::basic_iostream<typename Base::char_type, typename Base::traits_type> {

	//	typedef basic_kernelbuf<Base> kernelbuf_type;
	//	typedef typename Base::char_type Ch;
	//	typedef typename Base::traits_type Tr;
	//	typedef std::basic_iostream<Ch,Tr> iostream_type;

	//	basic_kstream(): iostream_type(nullptr), _kernelbuf()
	//	{ this->init(&_kernelbuf); }

	//	void
	//	begin_packet() {
	//		_kernelbuf.begin_packet();
	//	}

	//	void
	//	end_packet() {
	//		_kernelbuf.end_packet();
	//	}

	//	bool
	//	update_state() {
	//		return _kernelbuf.update_state();
	//	}

	//private:
	//	kernelbuf_type _kernelbuf;
	//};


	//typedef basic_kstream<char> kstream;

}

namespace stdx {

	template<class Base>
	struct type_traits<factory::basic_kernelbuf<Base>> {
		static constexpr const char*
		short_name() { return "kernelbuf"; }
		typedef sysx::buffer_category category;
	};

}

#endif // FACTORY_KERNELBUF_HH
