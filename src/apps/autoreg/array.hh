#ifndef APPS_AUTOREG_ARRAY_HH
#define APPS_AUTOREG_ARRAY_HH

#include <array>
#include <valarray>

namespace autoreg {

	template<class T, size_t N=1>
	struct Array {

		typedef size_t index_type;
		typedef Array<T, N-1> value_type;
		typedef std::array<index_type, N> size_type;

		Array(): _data() {}
		Array(const Array<T, N>& rhs): _data(rhs._data) {}
		template<size_t M> explicit Array(const std::array<index_type, M>& dims): _data(value_type(dims), dims[N-1]) {}

		const value_type& operator[](index_type i) const { return _data[i]; }
		value_type& operator[](index_type i) { return _data[i]; }
		void resize(const size_type& dims) {
			index_type n = dims.size_type::value();
			_data.resize(n);
			for (index_type i=0; i<n; ++i) {
				_data[i].resize(dims);
			}
		}

		template<class F>
		void operator()(F func) {
//			size_type idx;
			index_type idx[N];
			operator()(func, idx);
		}

		template<class F, class Idx>
		void operator()(F func, Idx idx) {
			index_type n = _data.size();
			for (index_type i=0; i<n; ++i) {
//				idx.size_type::value(i);
				idx[N-1] = i;
				_data[i](func, idx);
			}
		}

	private:
		std::valarray<value_type> _data;
	};

	template<class T>
	struct Array<T, 1> {

		typedef size_t index_type;
		typedef T value_type;
		typedef std::array<index_type, 1> size_type;

		Array(): _data() {}
		Array(const Array<T, 1>& rhs): _data(rhs._data) {}
		template<size_t M>
		explicit Array(std::array<index_type, M> dims): _data(T(), dims[0]) {}

		const value_type& operator[](index_type i) const { return _data[i]; }
		value_type& operator[](index_type i) { return _data[i]; }
		void resize(const size_type& dims) {
			index_type n = dims[0];
			_data.resize(n);
		}

		template<class F, class Idx>
		void operator()(F func, Idx idx) {
			index_type n = _data.size();
			for (index_type i=0; i<n; ++i) {
				idx[0] = i;
				func(_data[i], idx);
			}
		}
	private:
		std::valarray<value_type> _data;
	};

}

#endif // APPS_AUTOREG_ARRAY_HH
