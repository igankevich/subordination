#ifndef STDX_ALGORITHM_HH
#define STDX_ALGORITHM_HH

namespace stdx {

	template<class It>
	void
	delete_each(It first, It last) {
		while (first != last) {
			delete *first;
			++first;
		}
	}

}

#endif // STDX_ALGORITHM_HH
