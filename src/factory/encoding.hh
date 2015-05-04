// This code is adopted from Omnifarious answer to Stackoverflow question.
// See: http://stackoverflow.com/questions/5288076/doing-base64-encoding-and-decoding-in-openssl-c
namespace factory {

	static const char BASE64_TABLE[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	static const unsigned char reverse_table[128] = {
	   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
	   52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
	   64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
	   64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
	};

	size_t base64_encoded_size(size_t len) {
		return ((len + 2) / 3) * 4;
	}

	size_t base64_max_decoded_size(size_t len) {
		return len == 0 ? 0 : ((len / 4) * 3);
	}
	
	template<class It, class Res>
	void base64_encode(It first, It last, Res result) {

		static_assert(sizeof(*first) == 1,
			"base64_encode() works for sequences of 1-byte sized types.");
	
		typedef typename std::remove_reference<decltype(*result)>::type char_type;
		using std::numeric_limits;
	
		const size_t binlen = static_cast<size_t>(last - first);
		if (binlen > (numeric_limits<size_t>::max() / 4u) * 3u) {
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
	size_t base64_decode(It first, It last, Res result) {

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
			if ((c > 127) || (c < 0) || (reverse_table[c] > 63)) {
				throw std::invalid_argument("This contains characters not legal in a base64 encoded string.");
			}
			accumulator = (accumulator << 6) | reverse_table[c];
			bits_collected += 6;
			if (bits_collected >= 8) {
				bits_collected -= 8;
				*result++ = static_cast<char_type>((accumulator >> bits_collected) & 0xffu);
			}
		}

		return static_cast<size_t>(result - start);
	}

}

// Code is taken from http://www.packetizer.com/security/sha1/

namespace factory {
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

		SHA1() {
			H[0] = 0x67452301;
			H[1] = 0xEFCDAB89;
			H[2] = 0x98BADCFE;
			H[3] = 0x10325476;
			H[4] = 0xC3D2E1F0;
		}
	
		template<class It>
		void result(It output) {
			static_assert(sizeof(*output) == 4,
				"SHA1::result() works for sequences of 4-byte sized types.");
			PadMessage();
			std::copy(H, H + 5, output);
		}
	
		template<class It>
		void input(It first, It last) {

			static_assert(sizeof(*first) == 1,
				"SHA1::input() works for sequences of 1-byte sized types.");

			if (first == last) return;

			while (first != last) {
				Message_Block[Message_Block_Index++] = *first;

				Length_Low += 8;
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
		inline uint32_t CircularShift(int bits, uint32_t word) {
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
	void sha1_encode(It first, It last, Res result) {
		SHA1 sha1;
		sha1.input(first, last);
		sha1.result(result);
	}
	
	enum struct Opcode: int8_t {
		CONT_FRAME   = 0x0,
		TEXT_FRAME   = 0x1,
		BINARY_FRAME = 0x2,
		CONN_CLOSE   = 0x8,
		PING		 = 0x9,
		PONG		 = 0xa
	};
	
	struct Web_socket_frame_header {
		uint16_t extlen : 16;
		uint16_t len	: 7;
		uint16_t mask   : 1;
		uint16_t opcode : 4;
		uint16_t rsv3   : 1;
		uint16_t rsv2   : 1;
		uint16_t rsv1   : 1;
		uint16_t fin	: 1;

		bool masked() const { return mask == 1; }
		bool is_binary_frame() const {
			return static_cast<Opcode>(opcode) == Opcode::BINARY_FRAME;
		}
		bool has_valid_opcode() const { return opcode >= 0x0 && opcode <= 0xf; }

		friend std::ostream& operator<<(std::ostream& out, const Web_socket_frame_header& rhs) {
			return out
				<< "fin=" << rhs.fin << ','
				<< "rsv1=" << rhs.rsv1 << ','
				<< "rsv2=" << rhs.rsv2 << ','
				<< "rsv3=" << rhs.rsv3 << ','
				<< "opcode=" << rhs.opcode << ','
				<< "mask=" << rhs.mask << ','
				<< "len=" << rhs.len << ','
				<< "extlen=" << rhs.extlen;
		}
	};
	
	template<class It, class Res>
	void websocket_encode(It first, It last, Res result) {

		static_assert(std::is_same<typename std::iterator_traits<Res>::iterator_category,
			std::output_iterator_tag>::value
			&& std::is_same<typename std::iterator_traits<It>::iterator_category,
			std::random_access_iterator_tag>::value, "websocket_encode is defined for input and output iterators only");

		size_t srclength = last - first;
		if (srclength == 0) return;
		// TODO: mask data with random key
		Web_socket_frame_header hdr = { 0 };
		size_t offset = 2;
		hdr.opcode = static_cast<uint16_t>(Opcode::BINARY_FRAME);
		hdr.fin = 1;
		if (srclength <= 125) {
			hdr.len = srclength;
		} else if (srclength > 125 && srclength < 65536) {
			hdr.len = 126;
			hdr.extlen = srclength;
//			Bytes<uint16_t> raw = srclength;
//			raw.to_network_format();
//			std::copy(raw.begin(), raw.end(), target + offset);
			offset += 2;
		} else {
			hdr.len = 127;
			// TODO: ???
//			Bytes<uint64_t> raw = srclength;
//			raw.to_network_format();
//			std::copy(raw.begin(), raw.end(), target + offset);
//			offset += 8;
		}
	
		Bytes<Web_socket_frame_header> bytes = hdr;
		bytes.to_network_format();
//		std::copy(bytes.begin(), bytes.begin() + offset, target);
	
//		Logger(Level::WEBSOCKET)
//			<< "assertions: " << std::boolalpha
//			<< (target[0] == (char)((static_cast<int>(opcode) & 0x0F) | 0x80))
//			<< (target[1] == (char)srclength)
//			<< std::endl;
//	
//		Logger(Level::WEBSOCKET)
//			<< "Header: "
//			<< (uint32_t)target[0] << ' ' << (uint32_t)target[1] << ' '
//			<< (uint32_t)bytes[0] << ' ' << (uint32_t)hdr.fin << ' '
//			<< (uint32_t)(unsigned char)((static_cast<uint32_t>(opcode) & 0x0F) | 0x80)
//			<< ' ' << (uint32_t)(char)srclength << ' '
//			<< ' ' << sizeof(hdr) << ' '
//			<< std::endl;
	
//		buffer.write(bytes.begin(), offset);
//		buffer.write(first, srclength);
		std::copy(bytes.begin(), bytes.begin() + offset, result);
		std::copy(first, last, result);
//		std::copy(src, src + srclength, target + offset);
	}

	template<class It, class Res>
	size_t websocket_decode(It first, It last, Res output, Opcode* opcode) {
		static const size_t MASK_SIZE = 4;
		size_t srclength = first - last;
		size_t header_len = 2;
		if (srclength < header_len) return 0;
		Bytes<Web_socket_frame_header> raw_hdr(first, first + header_len);
		raw_hdr.to_host_format();
		if (raw_hdr.value().len == 126) {
			raw_hdr.to_network_format();
			std::copy(first + header_len, first + header_len + 2,
				raw_hdr.begin() + header_len);
			header_len += 2;
			raw_hdr.to_host_format();
		}
		Web_socket_frame_header hdr = raw_hdr;
		size_t payload_length = hdr.len == 126 ? hdr.extlen : hdr.len;
		Logger(Level::WEBSOCKET) << "recv header " << hdr << std::endl;
		*opcode = static_cast<Opcode>(hdr.opcode);
		if (hdr.masked()) header_len += MASK_SIZE;
		if (srclength < payload_length + header_len
			|| !hdr.has_valid_opcode()
			|| !hdr.is_binary_frame())
		{
			return 0;
		}
		if (hdr.masked()) {
			Bytes<char[MASK_SIZE]> mask(
				first + header_len - MASK_SIZE, first + header_len);
			size_t i = 0;
			std::transform(first + header_len, first + header_len + payload_length, output,
				[&i,&mask] (char ch) {
					ch ^= mask[i%4]; ++i; return ch;
				}
			);
//			std::for_each(first + header_len, first + header_len + payload_length,
//				[&i,&mask] (char& ch) { ch ^= mask[i%4]; ++i; }
//			);
//			output.write(first + header_len, payload_length);
		} else {
			std::copy(first + header_len, first + header_len + payload_length, output);
//			output.write(first + header_len, payload_length);
		}
		return header_len + payload_length;
	}

}
