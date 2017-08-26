#ifndef APPS_AUTOREG_DISCRETE_FUNCTION_HH
#define APPS_AUTOREG_DISCRETE_FUNCTION_HH

#include "array.hh"

namespace autoreg {

	template<class T, size_t N>
	struct Discrete_function {

		typedef size_t index_type;
		typedef std::array<index_type, N> size_type;
		typedef std::array<T, N> M;
		typedef typename Array<T, N>::value_type value_type;

		Discrete_function(): _size(), _min(), _max(), _array() {}
		Discrete_function(const Discrete_function<T, N>& rhs):
			_size(rhs._size), _min(rhs._min), _max(rhs._max), _array(rhs._array) {}
		explicit Discrete_function(const size_type& size): _size(size), _min(), _max(convert(size)), _array(_size) {}
		Discrete_function(const size_type& size, const M& min, const M& max): _size(size), _min(min), _max(max), _array(_size) {}

		const value_type& operator[](index_type i) const { return _array[i]; }
		value_type& operator[](index_type i) { return _array[i]; }

		const size_type& size() const { return _size; }
		void resize(const size_type& dims) {
			_array.resize(dims);
		}

		void read(std::istream& in) {
			index_type ndims;
			in.read((char*)&ndims, sizeof(ndims));
			if (ndims != N) {
				std::stringstream msg;
				msg << "Wrong number of dimensions while reading array from the stream: should be "
				    << N << ", but it is " << ndims;
				throw std::runtime_error(msg.str());
			}
			size_type dims;
			in.read((char*)&dims, dims.size()*sizeof(index_type));
			resize(dims);
			_array([&in] (T& val, index_type*) { in.read((char*)&val, sizeof(val)); });
		}

		void write(std::ostream& out) {
			size_type dims = size();
			index_type ndims = N;
			out.write((char*)&ndims, sizeof(ndims));
			out.write((char*)&dims, dims.size()*sizeof(index_type));
			_array([&out] (T val, index_type*) { out.write((char*)&val, sizeof(val)); });
		}

		template<class F>
		void operator()(F func) {
			_array(func);
		}

		M domain_point(index_type idx[N]) const {
			M pt;
			for (index_type i=0; i<N; ++i) {
				pt[i] = _min[i] + (_max[i] - _min[i]) * idx[i] / (_size[i] - 0);
			}
			return pt;
		}

		M domain_delta() const {
			M pt;
			for (index_type i=0; i<N; ++i) {
				pt[i] = (_max[i] - _min[i]) / (_size[i] - 1);
			}
			return pt;
		}

		void min(const M& min) { _min = min; }
		void max(const M& max) { _max = max; }

		const M& min() const { return _min; }
		const M& max() const { return _max; }

	private:

		static M convert(const size_type& v) {
			M x;
			for (index_type i=0; i<N; ++i) {
				x[i] = v[i];
			}
			return x;
		}

		size_type _size;
		M _min;
		M _max;
		Array<T, N> _array;
	};

}

#endif // vim:filetype=cpp
