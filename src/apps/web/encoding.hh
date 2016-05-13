#ifndef APPS_WEB_ENCODING_HH
#define APPS_WEB_ENCODING_HH

#include <cstddef> // offsetof

#include <sys/endpoint.hh>
#include <stdx/random.hh>
#include <factory/error.hh>

// This code is adapted from Omnifarious answer to Stackoverflow question.
// See: http://stackoverflow.com/questions/5288076/doing-base64-encoding-and-decoding-in-openssl-c
namespace factory {

	constexpr size_t
	base64_encoded_size(size_t len) noexcept {
		return ((len + 2u) / 3u) * 4u;
	}

	constexpr size_t
	base64_max_decoded_size(size_t len) noexcept {
		return (len / 4u) * 3u;
	}

	template<class It, class Res>
	void
	base64_encode(It first, It last, Res result) {

		constexpr static const char BASE64_TABLE[65] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		static_assert(sizeof(*first) == 1,
			"base64_encode() works for sequences of 1-byte sized types.");

		typedef typename std::remove_reference<decltype(*result)>::type char_type;

		const size_t binlen = static_cast<size_t>(last - first);
		if (binlen > base64_max_decoded_size(std::numeric_limits<size_t>::max())) {
			throw std::length_error("Converting too large a string to base64.");
		}

		Res last_result = result + std::ptrdiff_t(base64_encoded_size(binlen));

		int bits_collected = 0;
		unsigned int accumulator = 0;

		while (first != last) {
			const unsigned int ch = static_cast<unsigned int>(*first);
			accumulator = (accumulator << 8) | (ch & 0xffu);
			bits_collected += 8;
			while (bits_collected >= 6) {
				bits_collected -= 6;
				*result = static_cast<char_type>(BASE64_TABLE[(accumulator >> bits_collected) & 0x3fu]);
				++result;
			}
			++first;
		}
		if (bits_collected > 0) { // Any trailing bits that are missing.
	//		  assert(bits_collected < 6);
			accumulator <<= 6 - bits_collected;
			*result = static_cast<char_type>(BASE64_TABLE[accumulator & 0x3fu]);
			++result;
		}
		while (result != last_result) {
			*result = '=';
			++result;
		}
	//	   assert(outpos >= (result.size() - 2));
	//	   assert(outpos <= result.size());
	//	   return result;
	}

	template<class It, class Res>
	size_t
	base64_decode(It first, It last, Res result) {

		constexpr static const unsigned char BASE64_REVERSE_TABLE[128] = {
		   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
		   52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
		   64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
		   64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
		};


		static_assert(sizeof(*first) == 1,
			"base64_decode() works for sequences of 1-byte sized types.");

		typedef typename std::remove_reference<decltype(*result)>::type char_type;

		int bits_collected = 0;
		unsigned int accumulator = 0;

		Res start = result;

		while (first != last) {
			const int c = *first;
			++first;
			if (std::isspace(c) || c == '=') {
				// Skip whitespace and padding. Be liberal in what you accept.
				continue;
			}
			if ((c > 127) || (c < 0) || (BASE64_REVERSE_TABLE[c] > 63)) {
				throw std::invalid_argument("This contains characters not legal in a base64 encoded string.");
			}
			accumulator = (accumulator << 6) | BASE64_REVERSE_TABLE[c];
			bits_collected += 6;
			if (bits_collected >= 8) {
				bits_collected -= 8;
				*result++ = static_cast<char_type>((accumulator >> bits_collected) & 0xffu);
			}
		}

		return static_cast<size_t>(result - start);
	}

	// Code is taken from http://www.packetizer.com/security/sha1/

	/*
	 *  sha1.h
	 *
	 *  Copyright (C) 1998, 2009
	 *  Paul E. Jones <paulej@packetizer.com>
	 *  All Rights Reserved.
	 *
	 *****************************************************************************
	 *  $Id: sha1.h 12 2009-06-22 19:34:25Z paulej $
	 *****************************************************************************
	 *
	 *  Description:
	 *	  This class implements the Secure Hashing Standard as defined
	 *	  in FIPS PUB 180-1 published April 17, 1995.
	 *
	 *	  Many of the variable names in this class, especially the single
	 *	  character names, were used because those were the names used
	 *	  in the publication.
	 *
	 *	  Please read the file sha1.cpp for more information.
	 *
	 */

	struct SHA1 {

		constexpr SHA1() noexcept:
			H{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0},
			Message_Block{} {}

		template<class It>
		inline void
		result(It output) noexcept {
			static_assert(sizeof(*output) == 4,
				"SHA1::result() works for sequences of 4-byte sized types.");
			PadMessage();
			std::copy(H, H + 5, output);
		}

		template<class It>
		void
		input(It first, It last) {

			static_assert(sizeof(*first) == 1,
				"SHA1::input() works for sequences of 1-byte sized types.");

			if (first == last) return;

			while (first != last) {
				Message_Block[Message_Block_Index++] = *first;

				Length_Low += 8;
				// TODO: bad practise
				if (Length_Low == 0) {
					Length_High++;
					if (Length_High == 0) {
						throw std::length_error("SHA1 input is too long.");
					}
				}

				if (Message_Block_Index == 64) {
					ProcessMessageBlock();
				}

				++first;
			}
		}

	private:

		/*
		 *  ProcessMessageBlock
		 *
		 *  Description:
		 *	  This function will process the next 512 bits of the message
		 *	  stored in the Message_Block array.
		 *
		 *  Parameters:
		 *	  None.
		 *
		 *  Returns:
		 *	  Nothing.
		 *
		 *  Comments:
		 *	  Many of the variable names in this function, especially the single
		 *	  character names, were used because those were the names used
		 *	  in the publication.
		 *
		 */
		void ProcessMessageBlock() {
			const uint32_t K[] =	{			   // Constants defined for SHA-1
				0x5A827999,
				0x6ED9EBA1,
				0x8F1BBCDC,
				0xCA62C1D6
			};
			uint32_t temp;					   // Temporary word value
			uint32_t W[80];					  // Word sequence
			uint32_t A, B, C, D, E;			  // Word buffers

			/*
			 *  Initialize the first 16 words in the array W
			 */
			for (int t = 0; t < 16; t++) {
				W[t] = ((uint32_t) Message_Block[t * 4]) << 24;
				W[t] |= ((uint32_t) Message_Block[t * 4 + 1]) << 16;
				W[t] |= ((uint32_t) Message_Block[t * 4 + 2]) << 8;
				W[t] |= ((uint32_t) Message_Block[t * 4 + 3]);
			}

			for (int t = 16; t < 80; t++) {
				W[t] = CircularShift(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
			}

			A = H[0];
			B = H[1];
			C = H[2];
			D = H[3];
			E = H[4];

			for (int t = 0; t < 20; t++) {
				temp = CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
				E = D;
				D = C;
				C = CircularShift(30,B);
				B = A;
				A = temp;
			}

			for (int t = 20; t < 40; t++) {
				temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
				E = D;
				D = C;
				C = CircularShift(30,B);
				B = A;
				A = temp;
			}

			for(int t = 40; t < 60; t++) {
				temp = CircularShift(5,A) +
					   ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
				E = D;
				D = C;
				C = CircularShift(30,B);
				B = A;
				A = temp;
			}

			for(int t = 60; t < 80; t++) {
				temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
				E = D;
				D = C;
				C = CircularShift(30,B);
				B = A;
				A = temp;
			}

			H[0] += A;
			H[1] += B;
			H[2] += C;
			H[3] += D;
			H[4] += E;

			Message_Block_Index = 0;
		}

		/*
		 *  PadMessage
		 *
		 *  Description:
		 *	  According to the standard, the message must be padded to an even
		 *	  512 bits.  The first padding bit must be a '1'.  The last 64 bits
		 *	  represent the length of the original message.  All bits in between
		 *	  should be 0.  This function will pad the message according to those
		 *	  rules by filling the message_block array accordingly.  It will also
		 *	  call ProcessMessageBlock() appropriately.  When it returns, it
		 *	  can be assumed that the message digest has been computed.
		 *
		 *  Parameters:
		 *	  None.
		 *
		 *  Returns:
		 *	  Nothing.
		 *
		 *  Comments:
		 *
		 */
		void PadMessage() {
			/*
			 *  Check to see if the current message block is too small to hold
			 *  the initial padding bits and length.  If so, we will pad the
			 *  block, process it, and then continue padding into a second block.
			 */
			if (Message_Block_Index > 55) {
				Message_Block[Message_Block_Index++] = 0x80;
				while(Message_Block_Index < 64) {
					Message_Block[Message_Block_Index++] = 0;
				}

				ProcessMessageBlock();

				while(Message_Block_Index < 56) {
					Message_Block[Message_Block_Index++] = 0;
				}
			} else {
				Message_Block[Message_Block_Index++] = 0x80;
				while(Message_Block_Index < 56) {
					Message_Block[Message_Block_Index++] = 0;
				}

			}

			/*
			 *  Store the message length as the last 8 octets
			 */
			Message_Block[56] = (Length_High >> 24) & 0xFF;
			Message_Block[57] = (Length_High >> 16) & 0xFF;
			Message_Block[58] = (Length_High >> 8) & 0xFF;
			Message_Block[59] = (Length_High) & 0xFF;
			Message_Block[60] = (Length_Low >> 24) & 0xFF;
			Message_Block[61] = (Length_Low >> 16) & 0xFF;
			Message_Block[62] = (Length_Low >> 8) & 0xFF;
			Message_Block[63] = (Length_Low) & 0xFF;

			ProcessMessageBlock();
		}

		/*
		 *  Performs a circular left shift operation
		 */
		constexpr static
		uint32_t
		CircularShift(int bits, uint32_t word) noexcept {
			return (word << bits) | (word >> (32-bits));
		}

		uint32_t H[5];                   // Message digest buffers

		uint32_t Length_Low = 0;         // Message length in bits
		uint32_t Length_High = 0;        // Message length in bits

		unsigned char Message_Block[64]; // 512-bit message blocks
		int Message_Block_Index = 0;     // Index into message block array
	};

	/*
	 *  sha1.cpp
	 *
	 *  Copyright (C) 1998, 2009
	 *  Paul E. Jones <paulej@packetizer.com>
	 *  All Rights Reserved.
	 *
	 *****************************************************************************
	 *  $Id: sha1.cpp 12 2009-06-22 19:34:25Z paulej $
	 *****************************************************************************
	 *
	 *  Description:
	 *	  This class implements the Secure Hashing Standard as defined
	 *	  in FIPS PUB 180-1 published April 17, 1995.
	 *
	 *	  The Secure Hashing Standard, which uses the Secure Hashing
	 *	  Algorithm (SHA), produces a 160-bit message digest for a
	 *	  given data stream.  In theory, it is highly improbable that
	 *	  two messages will produce the same message digest.  Therefore,
	 *	  this algorithm can serve as a means of providing a "fingerprint"
	 *	  for a message.
	 *
	 *  Portability Issues:
	 *	  SHA-1 is defined in terms of 32-bit "words".  This code was
	 *	  written with the expectation that the processor has at least
	 *	  a 32-bit machine word size.  If the machine word size is larger,
	 *	  the code should still function properly.  One caveat to that
	 *	  is that the input functions taking characters and character arrays
	 *	  assume that only 8 bits of information are stored in each character.
	 *
	 *  Caveats:
	 *	  SHA-1 is designed to work with messages less than 2^64 bits long.
	 *	  Although SHA-1 allows a message digest to be generated for
	 *	  messages of any number of bits less than 2^64, this implementation
	 *	  only works with messages with a length that is a multiple of 8
	 *	  bits.
	 *
	 */
	template<class It, class Res>
	void
	sha1_encode(It first, It last, Res result) {
		SHA1 sha1;
		sha1.input(first, last);
		sha1.result(result);
	}

	enum struct Opcode: int8_t {
		cont_frame   = 0x0,
		text_frame   = 0x1,
		binary_frame = 0x2,
		conn_close   = 0x8,
		ping		 = 0x9,
		pong		 = 0xa
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
			uint16_t fin	 : 1;
			uint8_t          : 0;
			uint16_t len	 : 7;
			uint16_t maskbit : 1;
			uint8_t          : 0;
			uint16_t extlen;
			uint8_t          : 0;
			uint32_t extlen2;
			uint8_t          : 0;
			uint64_t footer; /// extlen3 (16) + mask (32) + padding (16)
		} hdr = {};
		unsigned char rawhdr[sizeof(hdr)];

		// check if header fields are aligned properly
		static_assert(offsetof(decltype(hdr), extlen)  == 2, "bad offset of hdr.extlen");
		static_assert(offsetof(decltype(hdr), extlen2) == 4, "bad offset of hdr.extlen2");
		static_assert(offsetof(decltype(hdr), footer)  == 8, "bad offset of hdr.footer");
		static_assert(sizeof(hdr) == 16, "bad websocket frame header size");

		template<class It>
		std::pair<It,It> decode(It first, It last) {
			// read first two bytes of a frame
			size_t input_size = last - first;
			if (input_size < constants::BASE_SIZE)
				return std::make_pair(first,last);
			std::copy(first, first + constants::BASE_SIZE, rawhdr);
			// keep reading until the end of the header
			size_t hdrsz = header_size();
			if (input_size < hdrsz) return std::make_pair(first,last);
			if (hdrsz > constants::BASE_SIZE) {
				std::copy(first + constants::BASE_SIZE,
					first + hdrsz, rawhdr + constants::BASE_SIZE);
			}
			size_t payload_sz = payload_size();
			if (input_size < payload_sz + hdrsz) {
				return std::make_pair(first,last);
			}
			return std::make_pair(first + hdrsz, first + hdrsz + payload_sz);
		}

		template<class It>
		size_t decode_header(It first, It last) {
			// read first two bytes of a frame
			size_t input_size = last - first;
			if (input_size < constants::BASE_SIZE)
				return 0;
			std::copy(first, first + constants::BASE_SIZE, rawhdr);
			// keep reading until the end of the header
			size_t hdrsz = header_size();
			if (input_size < hdrsz) return 0;
			if (hdrsz > constants::BASE_SIZE) {
				std::copy(first + constants::BASE_SIZE,
					first + hdrsz, rawhdr + constants::BASE_SIZE);
			}
			return hdrsz;
		}

		template<class Res>
		void encode(Res result) const {
			std::copy(rawhdr, rawhdr + header_size(), result);
		}

		void fin(int rhs) { hdr.fin = rhs; }

		void opcode(Opcode rhs) { hdr.opcode = static_cast<uint16_t>(rhs); }
		constexpr Opcode opcode() const { return static_cast<Opcode>(hdr.opcode); }
		constexpr bool is_masked() const { return hdr.maskbit == 1; }
		constexpr bool is_binary() const {
			return opcode() == Opcode::binary_frame;
		}
		constexpr bool has_valid_opcode() const { return hdr.opcode >= 0x0 && hdr.opcode <= 0xf; }

		constexpr size_t extlen_size() const {
			using namespace constants;
			return hdr.len == LEN16_TAG ? sizeof(Len16) :
				hdr.len == LEN64_TAG ? sizeof(Len64) : 0;
		}

		Len64 payload_size() const {
			using namespace constants;
			switch (hdr.len) {
				case LEN16_TAG: return sys::to_host_format<Len16>(hdr.extlen);
				case LEN64_TAG: {
					sys::Bytes<Len64> tmp(rawhdr + BASE_SIZE,
						rawhdr + BASE_SIZE + sizeof(Len64));
					tmp.to_host_format();
					return tmp.value();
				}
				default:
					return hdr.len;
			}
		}

		void payload_size(Len64 rhs) {
			using namespace constants;
			if (rhs <= 125) {
				hdr.len = rhs;
			} else if (rhs > 125 && rhs <= std::numeric_limits<Len16>::max()) {
				hdr.len = LEN16_TAG;
				sys::Bytes<Len16> raw = rhs;
				raw.to_network_format();
				hdr.extlen = raw;
			} else {
				hdr.len = LEN64_TAG;
				sys::Bytes<Len64> raw = rhs;
				raw.to_network_format();
				std::copy(raw.begin(), raw.end(), rawhdr + BASE_SIZE);
			}
		}

		constexpr size_t header_size() const {
			using namespace constants;
			return BASE_SIZE + extlen_size() + (is_masked() ? MASK_SIZE : 0);
		}

		Mask mask() const {
			using namespace constants;
			switch (hdr.len) {
				case LEN16_TAG: return hdr.extlen2;
				case LEN64_TAG: return sys::Bytes<Mask>(rawhdr + header_size() - MASK_SIZE,
						rawhdr + header_size());
				default:
					return sys::Bytes<Mask>(rawhdr + BASE_SIZE,
						rawhdr + BASE_SIZE + MASK_SIZE);
			}
		}

		void mask(Mask rhs) {
			using namespace constants;
			if (rhs == 0) {
				hdr.maskbit = 0;
			} else {
				hdr.maskbit = 1;
				switch (hdr.len) {
					case LEN16_TAG: hdr.extlen2 = rhs; break;
					case LEN64_TAG: {
						sys::Bytes<Mask> tmp = rhs;
						std::copy(tmp.begin(), tmp.end(), rawhdr + header_size() - MASK_SIZE);
						break;
					}
					default: {
						sys::Bytes<Mask> tmp = rhs;
						std::copy(tmp.begin(), tmp.end(), rawhdr + BASE_SIZE);
						break;
					}
				}
			}
		}

		template<class It, class Res>
		void copy_payload(It first, It last, Res result) const {
			using namespace constants;
			if (is_masked()) {
				sys::Bytes<Mask> m = mask();
				size_t i = 0;
				std::transform(first, last, result,
					[&i,&m] (char ch) {
						ch ^= m[i%4]; ++i; return ch;
					}
				);
			} else {
				std::copy(first, last, result);
			}
		}

		template<class It, class Res>
		char getpayloadc(It first, size_t nread) const {
			using namespace constants;
			if (is_masked()) {
				sys::Bytes<Mask> m = mask();
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
					<< "mask=" << sys::Bytes<Mask>(rhs.mask()) << ','
					<< "payload_size=" << rhs.payload_size() << ','
					<< "header_size=" << rhs.header_size();
			}
			return out;
		}
	};

	struct websocket_category {};
	struct websocket_encode_t {};
	struct websocket_decode_t {};

	template<class It, class Res, class Random>
	void websocket_encode(It first, It last, Res result, Random& rng) {
		size_t input_size = last - first;
		if (input_size == 0) return;
		Web_socket_frame frame;
		frame.opcode(Opcode::binary_frame);
		frame.fin(1);
		frame.payload_size(input_size);
		frame.mask(stdx::n_random_bytes<Web_socket_frame::Mask>(rng));
		frame.encode(result);
		frame.copy_payload(first, last, result);
		#ifndef NDEBUG
		stdx::dbg << stdx::make_trace("wbs", "send frame", frame);
		#endif
	}


	template<class It, class Res>
	size_t websocket_decode(It first, It last, Res output, Opcode* opcode) {
		Web_socket_frame frame;
		std::pair<It,It> payload = frame.decode(first, last);
		if (payload.first == first) return 0;
		#ifndef NDEBUG
		stdx::dbg << stdx::make_trace("wbs", "recv frame", frame);
		#endif
		*opcode = frame.opcode();
		// ignore non-binary and invalid frames
		if (frame.is_binary() && frame.has_valid_opcode()) {
			frame.copy_payload(payload.first, payload.second, output);
		}
		return payload.second - first;
	}

	template<class Res>
	void websocket_accept_header(const std::string& web_socket_key, Res header) {
		static const char WEBSOCKET_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		static const size_t SHA_DIGEST_SIZE = 5;
		typedef sys::Bytes<uint32_t, unsigned char> Digest_item;
		sys::Bytes<Digest_item[SHA_DIGEST_SIZE], unsigned char> hash;
		SHA1 sha1;
		sha1.input(web_socket_key.begin(), web_socket_key.end());
		sha1.input(WEBSOCKET_GUID, WEBSOCKET_GUID + sizeof(WEBSOCKET_GUID)-1);
		sha1.result(hash.value());
		std::for_each(hash.value(), hash.value() + SHA_DIGEST_SIZE,
			std::mem_fun_ref(&Digest_item::to_network_format));
		base64_encode(hash.begin(), hash.end(), header);
	}

	template<class Res, class Random>
	void websocket_key(Res key, Random& rng) {
		sys::Bytes<std::uint128_t> buf = stdx::n_random_bytes<std::uint128_t>(rng);
		base64_encode(buf.begin(), buf.end(), key);
	}

}

#endif // APPS_WEB_ENCODING_HH
