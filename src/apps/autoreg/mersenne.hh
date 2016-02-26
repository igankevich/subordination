#ifndef APPS_AUTOREG_MERSENNE_HH
#define APPS_AUTOREG_MERSENNE_HH

#include "exceptions.hh"

namespace autoreg {

	typedef struct{
	  unsigned int matrix_a;
	  unsigned int mask_b;
	  unsigned int mask_c;
	  unsigned int seed;
	} mt_struct_stripped;


	const unsigned int DCMT_SEED    = 4172;
	const size_t       MT_RNG_COUNT = 4096;
	const int          MT_MM        = 9;
	const int          MT_NN        = 19;
	const unsigned int MT_WMASK     = 0xFFFFFFFFU;
	const unsigned int MT_UMASK     = 0xFFFFFFFEU;
	const unsigned int MT_LMASK     = 0x1U;
	const int          MT_SHIFT0    = 12;
	const int          MT_SHIFTB    = 7;
	const int          MT_SHIFTC    = 15;
	const int          MT_SHIFT1    = 18;

	//const mt_struct_stripped* create_mt_config(unsigned int seed);
//	extern mt_struct_stripped MT_CONFIGURATION[MT_RNG_COUNT];
}



namespace {

using namespace autoreg;

// считывает параметры Вихря Мерсена из файла
void read_mt_params(mt_struct_stripped* h_MT, const char *filename) {

  	std::ifstream in(filename, std::ios::binary);
	if (!in.is_open()) {
		throw File_not_found(filename);
	}

	for (size_t i = 0; i < MT_RNG_COUNT; i++)
	    in.read((char*)&h_MT[i], sizeof(mt_struct_stripped));

#ifdef DISABLE_RANDOM_SEED
	const unsigned int seed = 0u;
#else
	const unsigned int seed = time(0);
#endif

	for(size_t i = 0; i < MT_RNG_COUNT; i++)
	    h_MT[i].seed = seed;
}

}


namespace autoreg {

//mt_struct_stripped MT_CONFIGURATION[MT_RNG_COUNT];

}

#endif // APPS_AUTOREG_MERSENNE_HH
