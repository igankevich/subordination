namespace factory {

	namespace components {

		template<class Base>
		struct basic_ikernelbuf: public Base {

			typedef Base base_type;
			using typename Base::int_type;
			using typename Base::traits_type;
			using typename Base::char_type;
			using typename Base::pos_type;
			typedef uint32_t size_type;
			typedef stdx::log<basic_ikernelbuf> this_log;

			enum struct State {
				READING_SIZE,
				BUFFERING_PAYLOAD,
				READING_PAYLOAD
			};

			basic_ikernelbuf() = default;
			basic_ikernelbuf(basic_ikernelbuf&&) = default;
			basic_ikernelbuf(const basic_ikernelbuf&) = delete;
			virtual ~basic_ikernelbuf() = default;

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for ibasic_kernelbuf");

			int_type underflow() {
				int_type ret = this->Base::underflow();
				this->update_state();
				if (this->_rstate == State::READING_PAYLOAD && this->egptr() != this->gptr()) {
					return *this->gptr();
				}
				return traits_type::eof();
			}

			std::streamsize xsgetn(char_type* s, std::streamsize n) {
				if (this->egptr() == this->gptr()) {
					this->Base::underflow();
				}
				this->update_state();
				if (this->egptr() == this->gptr() || this->state() != State::READING_PAYLOAD) {
					return std::streamsize(0);
				}
				return this->Base::xsgetn(s, n);
			}

//			template<class X> friend class basic_okernelbuf;
			template<class X> friend void append_payload(std::streambuf& buf, basic_ikernelbuf<X>& kbuf);

		private:

			pos_type packetpos() const { return this->_packetpos; }
			pos_type payloadpos() const { return this->_packetpos + static_cast<pos_type>(this->hdrsize()); }
			size_type packetsize() const { return this->_packetsize; }
			size_type bytes_left_until_the_end_of_the_packet() const {
				return this->_packetsize - (this->gptr() - (this->eback() + this->payloadpos()));
			}

			void update_state() {
				this->dumpstate();
				State old_state = this->state();
				switch (this->state()) {
					case State::READING_SIZE: this->read_kernel_packetsize(); break;
					case State::BUFFERING_PAYLOAD: this->buffer_payload(); break;
					case State::READING_PAYLOAD: this->read_payload(); break;
				}
				if (old_state != this->state()) {
					this->update_state();
				}
			}

			void read_kernel_packetsize() {
				size_type count = this->egptr() - this->gptr();
				if (count >= this->hdrsize()) {
					bits::Bytes<size_type> size(this->gptr(), this->gptr() + this->hdrsize());
					size.to_host_format();
					this->gbump(this->hdrsize());
					this->setpos(this->readoff());
					this->setsize(size);
					this->dumpstate();
					this->sets(State::BUFFERING_PAYLOAD);
				}
			}

			void buffer_payload() {
				pos_type endpos = this->egptr() - this->eback();
				if (this->_oldendpos < endpos) {
					this->_oldendpos = endpos;
				}
				if (this->readend() - this->packetpos() >= this->payloadsize()) {
					char_type* pos = this->eback() + this->packetpos();
					this->setg(this->eback(), pos, pos + this->payloadsize());
					this->sets(State::READING_PAYLOAD);
				} else {
					this->setg(this->eback(), this->egptr(), this->egptr());
				}
			}

			void read_payload() {
				pos_type off = this->readoff();
				if (off - this->packetpos() == this->payloadsize()) {
					pos_type endpos = this->egptr() - this->eback();
					if (this->_oldendpos > endpos) {
						this->setg(this->eback(), this->gptr(), this->eback() + this->_oldendpos);
					}
					this->_oldendpos = 0;
					this->sets(State::READING_SIZE);
				}
			}

			void dumpstate() {
				this_log() << std::setw(20) << std::left << this->state()
					<< "pptr=" << this->pptr() - this->pbase()
					<< ",epptr=" << this->epptr() - this->pbase()
					<< ",gptr=" << this->gptr() - this->eback()
					<< ",egptr=" << this->egptr() - this->eback()
					<< ",size=" << this->packetsize()
					<< ",start=" << this->packetpos()
					<< ",oldpos=" << this->_oldendpos
					<< std::endl;
			}

			friend std::ostream& operator<<(std::ostream& out, State rhs) {
				switch (rhs) {
					case State::READING_SIZE: out << "READING_SIZE"; break;
					case State::BUFFERING_PAYLOAD: out << "BUFFERING_PAYLOAD"; break;
					case State::READING_PAYLOAD: out << "READING_PAYLOAD"; break;
					default: break;
				}
				return out;
			}

			void sets(State rhs) {
				this_log() << "oldstate=" << this->_rstate
					<< ",newstate=" << rhs << std::endl;
				this->_rstate = rhs;
			}
			State state() const { return this->_rstate; }
			pos_type readoff() {
				return this->seekoff(0, std::ios_base::cur, std::ios_base::in);
			}
			pos_type readend() {
				return this->egptr() - this->eback();
//				return this->seekoff(0, std::ios_base::end, std::ios_base::in);
			}
			void setsize(size_type rhs) { this->_packetsize = rhs; }
			size_type payloadsize() const { return this->_packetsize - this->hdrsize(); }
			static constexpr
			size_type hdrsize() { return sizeof(_packetsize); }
			void setpos(pos_type rhs) { this->_packetpos = rhs; }

			size_type _packetsize = 0;
			pos_type _packetpos = 0;
			pos_type _oldendpos = 0;
			State _rstate = State::READING_SIZE;
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

			using typename Base::int_type;
			using typename Base::traits_type;
			using typename Base::char_type;
			using typename Base::pos_type;
			typedef uint32_t size_type;
			typedef stdx::log<basic_okernelbuf> this_log;

			enum struct State {
				WRITING_SIZE,
				WRITING_PAYLOAD,
				FINALISING
			};

			static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
				"bad base class for basic_okernelbuf");

			basic_okernelbuf() = default;
			basic_okernelbuf(const basic_okernelbuf&) = delete;
			basic_okernelbuf(basic_okernelbuf&&) = default;
			virtual ~basic_okernelbuf() { this->end_packet(); }

			int sync() {
				int ret = this->finalise();
				this->end_packet();
				return ret;
			}

			int_type overflow(int_type c) {
				int_type ret = this->Base::overflow(c);
				this->begin_packet();
				return ret;
			}

			std::streamsize xsputn(const char_type* s, std::streamsize n) {
				this->begin_packet();
				return this->Base::xsputn(s, n);
			}

//			template<class X>
//			void append_packet(basic_ikernelbuf<X>& rhs) {
//				this->Base::xsputn(rhs.eback() + rhs.packetpos(), rhs.packetsize());
//				rhs.gbump(rhs.packetsize());
//			}

		private:

			void begin_packet() {
				if (this->state() == State::FINALISING) {
					this->sets(State::WRITING_SIZE);
				}
				if (this->state() == State::WRITING_SIZE) {
					this->sets(State::WRITING_PAYLOAD);
					this->setbeg(this->writepos());
					this->putsize(0);
					this_log() << "begin_packet()     "
						<< "pbase=" << (void*)this->pbase()
						<< ", pptr=" << (void*)this->pptr()
						<< ", eback=" << (void*)this->eback()
						<< ", gptr=" << (void*)this->gptr()
						<< ", egptr=" << (void*)this->egptr()
						<< std::endl;
				}
			}

			void end_packet() {
				if (this->state() == State::WRITING_PAYLOAD) {
					pos_type end = this->writepos();
					size_type s = end - this->_begin;
					if (s == sizeof(size_type)) {
						this->pbump(-static_cast<std::make_signed<size_type>::type>(s));
					} else {
						this->seekpos(this->_begin, std::ios_base::out);
						this->putsize(s);
						this->seekpos(end, std::ios_base::out);
					}
					this_log() << "end_packet(): size=" << s << std::endl;
					this->sets(State::FINALISING);
				}
			}

			int finalise() {
				int ret = -1;
				if (this->state() == State::FINALISING) {
					this_log() << "finalise()" << std::endl;
					ret = this->Base::sync();
					if (ret == 0) {
						this->sets(State::WRITING_SIZE);
					}
				}
				return ret;
			}

			void putsize(size_type s) {
				bits::Bytes<size_type> pckt_size(s);
				pckt_size.to_network_format();
				this->Base::xsputn(pckt_size.begin(), pckt_size.size());
			}

			void setbeg(pos_type rhs) { this->_begin = rhs; }
			pos_type writepos() {
				return this->seekoff(0, std::ios_base::cur, std::ios_base::out);
			}

			void sets(State rhs) {
				this_log() << "oldstate=" << this->_state << ",newstate=" << rhs << std::endl;
				this->_state = rhs;
			}
			State state() const { return this->_state; }

			friend std::ostream& operator<<(std::ostream& out, State rhs) {
				switch (rhs) {
					case State::WRITING_SIZE: out << "WRITING_SIZE"; break;
					case State::WRITING_PAYLOAD: out << "WRITING_PAYLOAD"; break;
					case State::FINALISING: out << "FINALISING"; break;
					default: break;
				}
				return out;
			}

			pos_type _begin = 0;
			State _state = State::WRITING_SIZE;
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

	}

	typedef components::basic_kstream<char> kstream;

}
