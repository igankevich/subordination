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
	//	      assert(bits_collected < 6);
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

