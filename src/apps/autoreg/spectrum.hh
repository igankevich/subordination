#ifndef APPS_AUTOREG_SPECTRUM_HH
#define APPS_AUTOREG_SPECTRUM_HH

namespace autoreg {

	typedef int Int;

//	template<class T, Int N>
//	struct Vec: public Vec<T, N-1> {
//
//		Vec(): Vec<T, N-1>(), _value() {}
//		Vec(T val): _value(val) {}
//		template<class ... TT> Vec(T val, TT... args): Vec<T, N-1>(args...), _value(val) {}
//
//		T value() const { return _value; }
//		void value(T val) { _value = val; }
//		Int size() const { return N; }
//
//		void read2(std::istream& in) {
//			Vec<T, N-1>::read2(in);
//			in >> _value;
//		}
//
//		void write2(std::ostream& out) const {
//			Vec<T, N-1>::write2(out);
//			out << Vec<T, 1>::SEPARATOR << _value;
//		}
//
//		void write(std::ostream& out) const {
//			Vec<T, N-1>::write(out);
//			out.write((char*)&_value, sizeof(_value));
//		}
//
//		void read(std::istream& in) {
//			Vec<T, N-1>::read(in);
//			in.read((char*)&_value, sizeof(_value));
//		}
//
//	private:
//		T _value;
//
//		friend std::istream& operator>>(std::istream& in, Vec<T, N>& rhs) {
//			rhs.read2(in); return in;
//		}
//
//		friend std::ostream& operator<<(std::ostream& out, const Vec<T, N>& rhs) {
//			rhs.write2(out); return out;
//		}
//	};
//
//	template<class T>
//	struct Vec<T, 1> {
//
//		Vec(): _value() {}
//		Vec(T val): _value(val) {}
//
//		T value() const { return _value; }
//		void value(T val) { _value = val; }
//		Int size() const { return 1; }
//
//		void read2(std::istream& in) {
//			in >> _value;
//		}
//
//		void write2(std::ostream& out) const {
//			out << _value;
//		}
//
//		void write(std::ostream& out) const {
//			out.write((char*)&_value, sizeof(_value));
//		}
//
//		void read(std::istream& in) {
//			in.read((char*)&_value, sizeof(_value));
//		}
//
//		static const char SEPARATOR = ',';
//
//	private:
//		T _value;
//
//		friend std::istream& operator>>(std::istream& in, Vec<T, 1>& rhs) {
//			rhs.read2(in); return in;
//		}
//
//		friend std::ostream& operator<<(std::ostream& out, const Vec<T, 1>& rhs) {
//			rhs.write2(out); return out;
//		}
//	};

	template<class T, Int N=1>
	struct Array {

		typedef Int I;
		typedef Array<T, N-1> Value;
		typedef std::array<I, N> V;

		Array(): _data() {}
		Array(const Array<T, N>& rhs): _data(rhs._data) {}
		template<size_t M> explicit Array(const std::array<I, M>& dims): _data(Value(dims), dims[N-1]) {}

		const Value& operator[](Int i) const { return _data[i]; }
		Value& operator[](Int i) { return _data[i]; }
		void resize(const V& dims) {
			Int n = dims.V::value();
			_data.resize(n);
			for (Int i=0; i<n; ++i) {
				_data[i].resize(dims);
			}
		}

		template<class F>
		void operator()(F func) {
//			V idx;
			Int idx[N];
			operator()(func, idx);
		}

		template<class F, class Idx>
		void operator()(F func, Idx idx) {
			Int n = _data.size();
			for (Int i=0; i<n; ++i) {
//				idx.V::value(i);
				idx[N-1] = i;
				_data[i](func, idx);
			}
		}

	private:
		std::valarray<Value> _data;
	};

	template<class T>
	struct Array<T, 1> {

		typedef Int I;
		typedef T Value;
		typedef std::array<I, 1> V;

		Array(): _data() {}
		Array(const Array<T, 1>& rhs): _data(rhs._data) {}
		template<size_t M>
		explicit Array(std::array<I, M> dims): _data(T(), dims[0]) {}

		const Value& operator[](Int i) const { return _data[i]; }
		Value& operator[](Int i) { return _data[i]; }
		void resize(const V& dims) {
			Int n = dims[0];
			_data.resize(n);
		}

		template<class F, class Idx>
		void operator()(F func, Idx idx) {
			Int n = _data.size();
			for (Int i=0; i<n; ++i) {
				idx[0] = i;
				func(_data[i], idx);
			}
		}
	private:
		std::valarray<Value> _data;
	};

	template<class T, Int N>
	struct Discrete_function {

		typedef std::array<Int, N> V;
		typedef std::array<T, N> M;
		typedef Int I;
		typedef typename Array<T, N>::Value Value;

		Discrete_function(): _size(), _min(), _max(), _array() {}
		Discrete_function(const Discrete_function<T, N>& rhs):
			_size(rhs._size), _min(rhs._min), _max(rhs._max), _array(rhs._array) {}
		explicit Discrete_function(const V& size): _size(size), _min(), _max(convert(size)), _array(_size) {}
		Discrete_function(const V& size, const M& min, const M& max): _size(size), _min(min), _max(max), _array(_size) {}

		const Value& operator[](Int i) const { return _array[i]; }
		Value& operator[](Int i) { return _array[i]; }

		const V& size() const { return _size; }
		void resize(const V& dims) {
			_array.resize(dims);
		}

		void read(std::istream& in) {
			Int ndims;
			in.read((char*)&ndims, sizeof(ndims));
			if (ndims != N) {
				std::stringstream msg;
				msg << "Wrong number of dimensions while reading array from the stream: should be "
				    << N << ", but it is " << ndims;
				throw std::runtime_error(msg.str());
			}
			V dims;
			in.read((char*)&dims, dims.size()*sizeof(Int));
			resize(dims);
			_array([&in] (T& val, Int*) { in.read((char*)&val, sizeof(val)); });
		}

		void write(std::ostream& out) {
			V dims = size();
			Int ndims = N;
			out.write((char*)&ndims, sizeof(ndims));
			out.write((char*)&dims, dims.size()*sizeof(Int));
			_array([&out] (T val, Int*) { out.write((char*)&val, sizeof(val)); });
		}

		template<class F>
		void operator()(F func) {
			_array(func);
		}

		M domain_point(I idx[N]) const {
			M pt;
			for (I i=0; i<N; ++i) {
				pt[i] = _min[i] + (_max[i] - _min[i]) * idx[i] / (_size[i] - 0);
			}
			return pt;
		}

		M domain_delta() const {
			M pt;
			for (I i=0; i<N; ++i) {
				pt[i] = (_max[i] - _min[i]) / (_size[i] - 1);
			}
			return pt;
		}

		void min(const M& min) { _min = min; }
		void max(const M& max) { _max = max; }

		const M& min() const { return _min; }
		const M& max() const { return _max; }

	private:

		static M convert(const V& v) {
			M x;
			for (I i=0; i<N; ++i) {
				x[i] = v[i];
			}
			return x;
		}

		V _size;
		M _min;
		M _max;
		Array<T, N> _array;
	};

	template<class T>
	struct Const {
		static constexpr T PI = std::acos(T(-1));
	};

	template<class T, class I>
	inline T frequency_spec(T omega, T omega_max, I n) {
		if (omega == T(0)) return T(0);
		T omega_1 = omega/omega_max;
		T omega_n = std::pow(omega_1, -n);
		return (n/omega_max) * omega_n * std::exp(-(n/(n-T(1))) * omega_1 * omega_n);
	}

	template<class T, class I>
	inline T directional_spec(T theta, T theta_max, I m) {
		return std::tgamma(m + I(1))
			/ (std::pow(2, m) * std::pow(std::tgamma((m+T(1))/T(2)), 2))
			* std::pow(std::cos(theta-theta_max), m);
	}

}

#endif // APPS_AUTOREG_SPECTRUM_HH
