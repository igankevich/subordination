#ifndef APPS_AUTOREG_VECTOR_N_HH
#define APPS_AUTOREG_VECTOR_N_HH

namespace autoreg {

using std::ostream;
using std::istream;
using std::clog;
using std::endl;

template <class T, size_t n>
class Vector {
	T coord[n];
public:
	typedef T Value;
	typedef T value_type;

	Vector() {
		for (size_t i=0; i<n; ++i)
			coord[i] = T(0);
	}
//	explicit Vector(const T* coords);
	template <class A> Vector(const Vector<A, n> &v) {
		for (size_t i=0; i<n; ++i)
			coord[i] = v[i];
	}

	template <class A> Vector(A x, A y, A z) { coord[0] = x; coord[1] = y; coord[2] = z; }
	template <class A> Vector(A x, A y) { coord[0] = x; coord[1] = y; }
	template <class A> explicit Vector(A x) {
		for (size_t i=0; i<n; ++i) {
			coord[i] = x;
		}
	}

	Vector<T, n> operator+(const Vector<T, n> &v) const { return trans(v, std::plus<T>()); }
	Vector<T, n> operator-(const Vector<T, n> &v) const { return trans(v, std::minus<T>()); }
	Vector<T, n> operator*(const Vector<T, n> &v) const { return trans(v, std::multiplies<T>()); }
	Vector<T, n> operator/(const Vector<T, n> &v) const { return trans(v, std::divides<T>()); }
	const Vector<T, n>& operator=(const T& rhs) {
	    for (size_t i=0; i<n; ++i)
            coord[i] = rhs;
	    return *this;
	}

	Vector<T, n>& operator=(const Vector<T, n> &v) {
		for (size_t i=0; i<n; ++i)
			coord[i] = v.coord[i];
		return *this;
	}
	//template <class V> Vector<T, n>& operator=(V val);

	bool operator==(const Vector<T, n> &v) const {
		for (size_t i=0; i<n; ++i)
			if (coord[i] != v.coord[i])
				return false;
		return true;
	}

	bool operator<(const Vector<T, n> &v) const {
		for (size_t i=0; i<n; ++i) {
			if (coord[i] < v.coord[i]) return true;
			if (coord[i] > v.coord[i]) return false;
		}
		return false;
	}

	inline void rot(size_t r) {
		std::rotate(&coord[0], &coord[r%n], &coord[n]);
	}
	inline T& operator[] (size_t i) { return coord[i]; }
	inline const T& operator[] (size_t i) const { return coord[i]; }
	inline operator size_t() const { return size(); }
	inline size_t size() const;
	template<class BinOp>
	inline T reduce(BinOp op) const;
	//inline operator const T*() const { return coord; }

	friend Vector<T, n> operator+(const Vector<T, n> &v, T val) { Vector<T, n> vec(v); vec.trans(bind2nd(std::plus<T>(), val)); return vec; }
	friend Vector<T, n> operator-(const Vector<T, n> &v, T val) { Vector<T, n> vec(v); vec.trans(bind2nd(std::minus<T>(), val)); return vec; }
	template <class V> friend Vector<T, n> operator*(const Vector<T, n> &v, V val) { Vector<T, n> vec(v); vec.trans(bind2nd(std::multiplies<V>(), val)); return vec; }
	friend Vector<T, n> operator/(const Vector<T, n> &v, T val) { Vector<T, n> vec(v); vec.trans(bind2nd(std::divides<T>(), val)); return vec; }

	template <class A, size_t m> friend ostream& operator<<(ostream &out, const Vector<A, m> &v);
	template <class A, size_t m> friend istream& operator>>(istream &in, Vector<A, m> &v);

	friend sysx::packetstream&
	operator<<(sysx::packetstream& out, const Vector& rhs) {
		for (size_t i=0; i<n; ++i) out << rhs.coord[i];
		return out;
	}

	friend sysx::packetstream&
	operator>>(sysx::packetstream& in, Vector& rhs) {
		for (size_t i=0; i<n; ++i) in >> rhs.coord[i];
		return in;
	}

	T*
	begin() noexcept {
		return coord;
	}

	T*
	end() noexcept {
		return coord + n;
	}

	const T*
	begin() const noexcept {
		return coord;
	}

	const T*
	end() const noexcept {
		return coord + n;
	}

private:
	template <class BinOp> inline Vector<T, n> trans(const Vector<T, n> &v, BinOp op) const;
	template <class UnOp> inline void trans(UnOp op) { std::transform(coord, coord+n, coord, op); }
} ;//__attribute__((aligned(16)));

template<typename T>
class Vector<T, 1> {
	T val;
public:
	Vector(): val(0) {}
	template <class A> Vector(A t): val(t) {}
	inline operator T&() { return val; }
	inline operator const T&() const { return val; }
	inline T& operator[] (size_t) { return val; }
	inline const T& operator[] (size_t) const { return val; }
};

typedef Vector<float, 3> float3;
typedef Vector<float, 2> float2;
typedef Vector<float, 1> float1;
typedef Vector<double, 3> double3;
typedef Vector<double, 2> double2;
typedef Vector<double, 1> double1;
typedef Vector<size_t, 3> size3;
typedef Vector<size_t, 2> size2;
typedef Vector<size_t, 1> size1;

template<size_t n=3> class Rot_index;
template<size_t n=3> class Index;
//template<size_t n=3> class Index_r;
//template<size_t n=3> class Index_zyx;

template<>
class Index<3> {
	const size3& s;
public:
	Index(const size3& sz): s(sz) {}
	size_t t(size_t i) const { return ((i/s[2])/s[1])%s[0]; }
	size_t x(size_t i) const { return (i/s[2])%s[1]; }
	size_t y(size_t i) const { return i%s[2]; }
	size_t operator()(size_t i, size_t j=0, size_t k=0) const {
//		if (i < 0 || i >= s[0]) { clog << size3(i, j, k) << endl; throw I(); }
//		if (j < 0 || j >= s[1]) { clog << size3(i, j, k) << endl; throw J(); }
//		if (k < 0 || k >= s[2]) { clog << size3(i, j, k) << endl; throw K(); }
		return (i*s[1] + j)*s[2] + k;
	}
	size_t operator()(const size3& v) const { return v[0]*s[1]*s[2] + v[1]*s[2] + v[2]; }
};

template<>
class Rot_index<3> {
	size3 s;
	size_t rot;
public:
	Rot_index(const size3& sz, size_t rotation): s(sz), rot(rotation%3) {
		s.rot(3-rot);
	}

	inline size_t operator()(size_t i, size_t j=0, size_t k=0) const {
		return this->operator()(size3(i, j, k));
	}
	inline size_t operator()(size3 v) const {
		v.rot(3-rot);
		return v[0]*s[1]*s[2] + v[1]*s[2] + v[2];
	}
};
template<>
class Index<1> {
	const size1& s;
public:
	Index(const size1& sz): s(sz) {}
	inline size_t operator()(size_t) const { return s[0]; }
};


template<>
class Index<2> {
	const size2& s;
public:
	Index(const size2& sz): s(sz) {}
	inline int operator()(int i, int j) const {
		//if (i < 0 || i >= s[0]) throw 1;
		//if (j < 0 || j >= s[1]) throw 2;
		return i*s[1] + j;
	}
};

//#define z(i, j, k) z[(k)*zlen_x*zlen_y + (i)*zlen_y + (j)]
//template<>
//class Index_r<3> {
//	const size3& s;
//public:
//	Index_r(const size3& sz): s(sz) {}
//	inline size_t operator()(size_t i, size_t j, size_t k) const { return k*s[0]*s[1] + i*s[1] + j; }
//	//inline size_t operator()(size_t i, size_t j=0, size_t k=0) const { return i*s[1]*s[2] + j*s[2] + k; }
//};

//template<>
//class Index_zyx<3> {
//	const size3& s;
//public:
//	Index_zyx(const size3& sz): s(sz) {}
//	inline size_t operator()(size_t i, size_t j, size_t k) const { return k*s[0]*s[1] + j*s[0] + i; }
//	//inline size_t operator()(size_t i, size_t j=0, size_t k=0) const { return i*s[1]*s[2] + j*s[2] + k; }
//};


// ------------------------------------------------------

template <class T, size_t n>
inline size_t Vector<T, n>::size() const {
	size_t s = coord[0];
	for (size_t i=1; i<n; ++i)
		s *= coord[i];
	return s;
}

template<class T, size_t n>
template<class BinOp>
inline T Vector<T, n>::reduce(BinOp op) const {
    T s = coord[0];
    for (size_t i=1; i<n; ++i)
        s = op(s, coord[i]);
    return s;
}

template <class T, size_t n>
template <class BinOp>
inline Vector<T, n> Vector<T, n>::trans(const Vector<T, n> &v, BinOp op) const {
	Vector <T, n> v2;
	std::transform(coord, coord+n, v.coord, v2.coord, op);
	return v2;
}

/*template <class T, size_t n> template <class V> Vector<T, n>& Vector<T, n>::operator=(V val) {
	std::fill(coord, coord+n, val);
}*/

template <class A, size_t m>
ostream& operator<<(ostream &out, const Vector<A, m> &v) {
	for (size_t i=0; i<m-1; ++i) out << v.coord[i] << ',';
	return out << v.coord[m-1];
}

template <class A, size_t m>
istream& operator>>(istream &in, Vector<A, m> &v) {
	for (size_t i=0; i<m; ++i) in >> v.coord[i], in.get();
	return in;
}


}

#endif // APPS_AUTOREG_VECTOR_N_HH
