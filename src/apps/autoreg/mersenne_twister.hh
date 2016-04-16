#ifndef APPS_AUTOREG_MERSENNE_TWISTER_HH
#define APPS_AUTOREG_MERSENNE_TWISTER_HH

#include <cstdint>
#include <limits>
#include <iterator>

namespace autoreg {

	// TODO 2016-04-13 implement std::seed_seq interaface for parallel MT
	struct parallel_mt {

		typedef uint32_t result_type;
		typedef size_t size_type;

		struct mt_struct_stripped {

			uint32_t matrix_a;
			uint32_t mask_b;
			uint32_t mask_c;
			uint32_t seed;

			friend std::istream&
			operator>>(std::istream& in, mt_struct_stripped& rhs) {
			    in.read((char*)&rhs, sizeof(mt_struct_stripped));
				return in;
			}

		};

		explicit
		parallel_mt(size_type i) {
			init_mt_params(i);
			init_mt_state();
		}

		result_type
		operator()() noexcept {
			iState1 = iState + 1;
			iStateM = iState + MT_MM;
			if (iState1 >= MT_NN) iState1 -= MT_NN;
			if (iStateM >= MT_NN) iStateM -= MT_NN;
			mti  = mti1;
			mti1 = mt[iState1];
			mtiM = mt[iStateM];

			// MT recurrence
			x = (mti & MT_UMASK) | (mti1 & MT_LMASK);
			x = mtiM ^ (x >> 1) ^ ((x & 1) ? matrix_a : 0);

			mt[iState] = x;
			iState = iState1;

			//Tempering transformation
			x ^= (x >> MT_SHIFT0);
			x ^= (x << MT_SHIFTB) & mask_b;
			x ^= (x << MT_SHIFTC) & mask_c;
			x ^= (x >> MT_SHIFT1);

			return x;
		}

		result_type
		min() const noexcept {
			return std::numeric_limits<result_type>::min();
		}

		result_type
		max() const noexcept {
			return std::numeric_limits<result_type>::max();
		}

		void
		seed(result_type rhs) noexcept {
			_seed = rhs;
		}

	private:

		void
		init_mt_params(size_type i) {
			if (i >= MT_RNG_COUNT) {
				throw std::invalid_argument("i is too large");
			}
			mt_struct_stripped d_MT[MT_RNG_COUNT];
			read_mt_params(d_MT, "MersenneTwister.dat");
			//Load bit-vector Mersenne Twister parameters
			matrix_a = d_MT[i].matrix_a;
			mask_b = d_MT[i].mask_b;
			mask_c = d_MT[i].mask_c;
		}

		// считывает параметры Вихря Мерсена из файла
		void
		read_mt_params(mt_struct_stripped* h_MT, const char *filename) {
		  	std::ifstream in(filename, std::ios::binary);
			if (!in.is_open()) {
				std::stringstream msg;
				msg << "file not found: " << filename;
				throw std::invalid_argument(msg.str());
			}
			std::copy_n(std::istream_iterator<mt_struct_stripped>(in), MT_RNG_COUNT, h_MT);
		}

		void
		init_mt_state() {

			//Initialize current state
			mt[0] = _seed;
			for (uint32_t i=1; i<MT_NN; ++i) {
				mt[i] = (1812433253U * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i) & MT_WMASK;
			}

			iState = 0;
			mti1 = mt[0];
		}

		static const unsigned int DCMT_SEED    = 4172;
		static const size_t       MT_RNG_COUNT = 4096;
		static const int          MT_MM        = 9;
		static const int          MT_NN        = 19;
		static const unsigned int MT_WMASK     = 0xFFFFFFFFU;
		static const unsigned int MT_UMASK     = 0xFFFFFFFEU;
		static const unsigned int MT_LMASK     = 0x1U;
		static const int          MT_SHIFT0    = 12;
		static const int          MT_SHIFTB    = 7;
		static const int          MT_SHIFTC    = 15;
		static const int          MT_SHIFT1    = 18;

		// MT params
		uint32_t matrix_a, mask_b, mask_c;
		uint32_t _seed = 0;

		// MT state
		int32_t iState, iState1, iStateM;
		uint32_t mti, mti1, mtiM, x;
		uint32_t mt[MT_NN];

	};

}

#endif // APPS_AUTOREG_MERSENNE_TWISTER_HH
