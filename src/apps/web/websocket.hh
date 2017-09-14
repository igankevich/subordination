#ifndef APPS_WEB_ENCODING_HH
#define APPS_WEB_ENCODING_HH

#include <cstddef>

#include <unistdx/base/log_message>
#include <unistdx/base/n_random_bytes>
#include <unistdx/net/bytes>

namespace factory {

	enum struct Opcode: int8_t {
		cont_frame   = 0x0,
		text_frame   = 0x1,
		binary_frame = 0x2,
		conn_close   = 0x8,
		ping         = 0x9,
		pong         = 0xa
	};

	namespace constants {
		static const size_t MASK_SIZE = 4;
		static const size_t BASE_SIZE = 2;
		static const uint16_t LEN16_TAG = 126;
		static const uint16_t LEN64_TAG = 127;
	}

	union Web_socket_frame {

		typedef uint32_t Mask;
		typedef uint16_t Len16;
		typedef uint64_t Len64;

		struct {
			uint16_t opcode  : 4;
			uint16_t rsv3    : 1;
			uint16_t rsv2    : 1;
			uint16_t rsv1    : 1;
			uint16_t fin     : 1;
			uint8_t          : 0;
			uint16_t len     : 7;
			uint16_t maskbit : 1;
			uint8_t          : 0;
			uint16_t extlen;
			uint8_t          : 0;
			uint32_t extlen2;
			uint8_t          : 0;
			uint64_t footer;        /// extlen3 (16) + mask (32) + padding (16)
		} hdr = {};
		unsigned char rawhdr[sizeof(hdr)];

		// check if header fields are aligned properly
		static_assert(
			offsetof(decltype(hdr), extlen)  == 2,
			"bad offset of hdr.extlen"
		);
		static_assert(
			offsetof(decltype(hdr), extlen2) == 4,
			"bad offset of hdr.extlen2"
		);
		static_assert(
			offsetof(decltype(hdr), footer)  == 8,
			"bad offset of hdr.footer"
		);
		static_assert(sizeof(hdr) == 16, "bad websocket frame header size");

		template<class It>
		std::pair<It,It>
		decode(It first, It last) {
			// read first two bytes of a frame
			size_t input_size = last - first;
			if (input_size < constants::BASE_SIZE)
				return std::make_pair(first,last);
			std::copy(first, first + constants::BASE_SIZE, rawhdr);
			// keep reading until the end of the header
			size_t hdrsz = header_size();
			if (input_size < hdrsz) return std::make_pair(first,last);
			if (hdrsz > constants::BASE_SIZE) {
				std::copy(
					first + constants::BASE_SIZE,
					first + hdrsz,
					rawhdr + constants::BASE_SIZE
				);
			}
			size_t payload_sz = payload_size();
			if (input_size < payload_sz + hdrsz) {
				return std::make_pair(first,last);
			}
			return std::make_pair(first + hdrsz, first + hdrsz + payload_sz);
		}

		template<class It>
		size_t
		decode_header(It first, It last) {
			// read first two bytes of a frame
			size_t input_size = last - first;
			if (input_size < constants::BASE_SIZE)
				return 0;
			std::copy(first, first + constants::BASE_SIZE, rawhdr);
			// keep reading until the end of the header
			size_t hdrsz = header_size();
			if (input_size < hdrsz) return 0;
			if (hdrsz > constants::BASE_SIZE) {
				std::copy(
					first + constants::BASE_SIZE,
					first + hdrsz,
					rawhdr + constants::BASE_SIZE
				);
			}
			return hdrsz;
		}

		template<class Res>
		void
		encode(Res result) const {
			std::copy(rawhdr, rawhdr + header_size(), result);
		}

		void
		fin(int rhs) {
			hdr.fin = rhs;
		}

		void
		opcode(Opcode rhs) {
			hdr.opcode = static_cast<uint16_t>(rhs);
		}

		constexpr Opcode
		opcode() const {
			return static_cast<Opcode>(hdr.opcode);
		}

		constexpr bool
		is_masked() const {
			return hdr.maskbit == 1;
		}

		constexpr bool
		is_binary() const {
			return opcode() == Opcode::binary_frame;
		}

		constexpr bool
		has_valid_opcode() const {
			return hdr.opcode >= 0x0 && hdr.opcode <= 0xf;
		}

		constexpr size_t
		extlen_size() const {
			using namespace constants;
			return hdr.len == LEN16_TAG ? sizeof(Len16) :
			       hdr.len == LEN64_TAG ? sizeof(Len64) : 0;
		}

		Len64
		payload_size() const {
			using namespace constants;
			switch (hdr.len) {
			case LEN16_TAG: return sys::to_host_format<Len16>(hdr.extlen);
			case LEN64_TAG: {
				sys::bytes<Len64> tmp(rawhdr + BASE_SIZE,
				                      rawhdr + BASE_SIZE + sizeof(Len64));
				tmp.to_host_format();
				return tmp.value();
			}
			default:
				return hdr.len;
			}
		}

		void
		payload_size(Len64 rhs) {
			using namespace constants;
			if (rhs <= 125) {
				hdr.len = rhs;
			} else if (rhs > 125 && rhs <= std::numeric_limits<Len16>::max()) {
				hdr.len = LEN16_TAG;
				sys::bytes<Len16> raw = rhs;
				raw.to_network_format();
				hdr.extlen = raw;
			} else {
				hdr.len = LEN64_TAG;
				sys::bytes<Len64> raw = rhs;
				raw.to_network_format();
				std::copy(raw.begin(), raw.end(), rawhdr + BASE_SIZE);
			}
		}

		constexpr size_t
		header_size() const {
			using namespace constants;
			return BASE_SIZE + extlen_size() + (is_masked() ? MASK_SIZE : 0);
		}

		Mask
		mask() const {
			using namespace constants;
			switch (hdr.len) {
			case LEN16_TAG: return hdr.extlen2;
			case LEN64_TAG: return sys::bytes<Mask>(
					rawhdr + header_size() - MASK_SIZE,
					rawhdr + header_size()
				);
			default:
				return sys::bytes<Mask>(
					rawhdr + BASE_SIZE,
					rawhdr + BASE_SIZE + MASK_SIZE
				);
			}
		}

		void
		mask(Mask rhs) {
			using namespace constants;
			if (rhs == 0) {
				hdr.maskbit = 0;
			} else {
				hdr.maskbit = 1;
				switch (hdr.len) {
				case LEN16_TAG: hdr.extlen2 = rhs; break;
				case LEN64_TAG: {
					sys::bytes<Mask> tmp = rhs;
					std::copy(
						tmp.begin(),
						tmp.end(),
						rawhdr + header_size() -
						MASK_SIZE
					);
					break;
				}
				default: {
					sys::bytes<Mask> tmp = rhs;
					std::copy(tmp.begin(), tmp.end(), rawhdr + BASE_SIZE);
					break;
				}
				}
			}
		}

		template<class It, class Res>
		void
		copy_payload(It first, It last, Res result) const {
			using namespace constants;
			if (is_masked()) {
				sys::bytes<Mask> m = mask();
				size_t i = 0;
				std::transform(
					first,
					last,
					result,
					[&i,&m] (char ch) {
					    ch ^= m[i%4]; ++i; return ch;
					}
				);
			} else {
				std::copy(first, last, result);
			}
		}

		template<class It, class Res>
		char
		getpayloadc(It first, size_t nread) const {
			using namespace constants;
			if (is_masked()) {
				sys::bytes<Mask> m = mask();
				return *first ^ m[nread%4];
			} else {
				return *first;
			}
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Web_socket_frame& rhs) {
			typedef Web_socket_frame::Mask Mask;
			std::ostream::sentry s(out);
			if (s) {
				out << "fin=" << rhs.hdr.fin << ','
				    << "rsv1=" << rhs.hdr.rsv1 << ','
				    << "rsv2=" << rhs.hdr.rsv2 << ','
				    << "rsv3=" << rhs.hdr.rsv3 << ','
				    << "opcode=" << rhs.hdr.opcode << ','
				    << "maskbit=" << rhs.hdr.maskbit << ','
				    << "len=" << rhs.hdr.len << ','
				    << "mask=" << sys::bytes<Mask>(rhs.mask()) << ','
				    << "payload_size=" << rhs.payload_size() << ','
				    << "header_size=" << rhs.header_size();
			}
			return out;
		}

	};

	template<class It, class Res, class Random>
	void
	websocket_encode(It first, It last, Res result, Random& rng) {
		size_t input_size = last - first;
		if (input_size == 0) return;
		Web_socket_frame frame;
		frame.opcode(Opcode::binary_frame);
		frame.fin(1);
		frame.payload_size(input_size);
		frame.mask(sys::n_random_bytes<Web_socket_frame::Mask>(rng));
		frame.encode(result);
		frame.copy_payload(first, last, result);
		#ifndef NDEBUG
		sys::log_message("wbs", "send _", frame);
		#endif
	}

	template<class It, class Res>
	size_t
	websocket_decode(It first, It last, Res output, Opcode* opcode) {
		Web_socket_frame frame;
		std::pair<It,It> payload = frame.decode(first, last);
		if (payload.first == first) return 0;
		#ifndef NDEBUG
		sys::log_message("wbs", "recv _", frame);
		#endif
		*opcode = frame.opcode();
		// ignore non-binary and invalid frames
		if (frame.is_binary() && frame.has_valid_opcode()) {
			frame.copy_payload(payload.first, payload.second, output);
		}
		return payload.second - first;
	}

	template<class Res>
	void
	websocket_accept_header(const std::string& web_socket_key, Res header) {
		static const char WEBSOCKET_GUID[] =
			"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		static const size_t SHA_DIGEST_SIZE = 5;
		typedef sys::bytes<uint32_t, unsigned char> Digest_item;
		sys::bytes<Digest_item[SHA_DIGEST_SIZE], unsigned char> hash;
		//SHA1 sha1;
		//sha1.input(web_socket_key.begin(), web_socket_key.end());
		//sha1.input(WEBSOCKET_GUID, WEBSOCKET_GUID + sizeof(WEBSOCKET_GUID)-1);
		//sha1.result(hash.value());
		std::for_each(
			hash.value(),
			hash.value() + SHA_DIGEST_SIZE,
			std::mem_fun_ref(&Digest_item::to_network_format)
		);
		//base64_encode(hash.begin(), hash.end(), header);
	}

	template<class Res, class Random>
	void
	websocket_key(Res key, Random& rng) {
		sys::bytes<sys::uint128_t> buf =
			sys::n_random_bytes<sys::uint128_t>(rng);
		//base64_encode(buf.begin(), buf.end(), key);
	}

}

#endif // vim:filetype=cpp
