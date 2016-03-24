#ifndef STDX_CSTRING_ITERATOR_HH
#define STDX_CSTRING_ITERATOR_HH

#include <memory>

namespace std {

	namespace bits {

		template<class Pointer>
		typename std::pointer_traits<Pointer>::difference_type
		cstring_arr_size(Pointer first) {
			typename std::pointer_traits<Pointer>::difference_type n=0;
			if (first) {
				while (*first) {
					++n;
					++first;
				}
			}
			return n;
		}

	}

	template<class T>
	T*
	begin(T* rhs) noexcept {
		return rhs;
	}

	template<class T>
	const T*
	begin(const T* rhs) noexcept {
		return rhs;
	}

	template<class T>
	T*
	end(T* rhs) noexcept {
		return rhs + bits::cstring_arr_size(rhs);
	}

	template<class T>
	const T*
	end(const T* rhs) noexcept {
		return rhs + bits::cstring_arr_size(rhs);
	}

}

#endif // STDX_CSTRING_ITERATOR_HH
