#ifndef STDX_FOR_EACH_HH
#define STDX_FOR_EACH_HH

#include <stdx/unlock_guard.hh>

namespace stdx {

	template<class It, class Pred, class Func>
	void
	for_each_if(It first, It last, Pred pred, Func func) {
		while (first != last) {
			if (pred(*first)) {
				func(*first);
			}
			++first;
		}
	}

	template<class Lock, class It, class Func>
	void
	for_each_thread_safe(Lock& lock, It first, It last, Func func) {
		while (first != last) {
			unlock_guard<Lock> unlock(lock);
			func(*first);
			++first;
		}
	}

	template<class It>
	void
	delete_each(It first, It last) {
		while (first != last) {
			delete *first;
			++first;
		}
	}

}

#endif // STDX_FOR_EACH_HH
