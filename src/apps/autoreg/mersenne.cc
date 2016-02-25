#include "mersenne.hh"
#include "exceptions.hh"

#include <fstream>

namespace {

using namespace autoreg;

// считывает параметры Вихря Мерсена из файла
const mt_struct_stripped* read_mt_params(const char *filename) {
	static mt_struct_stripped h_MT[MT_RNG_COUNT];

	using namespace std;
  	ifstream in(filename, ios::binary);
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
	
	return h_MT;
}

}


namespace autoreg {

const mt_struct_stripped* MT_CONFIGURATION = read_mt_params("MersenneTwister.dat");

}
