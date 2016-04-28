#ifndef APPS_AUTOREG_EMATH_HH
#define APPS_AUTOREG_EMATH_HH

#include <cmath>
#include <valarray>
#include <iostream>
#include <iomanip>
#include "domain.hh"

namespace autoreg {

const size_t MAX_POLY_ORDER = 60;

/// Функция вычисляет значение полинома Эрмита Hn(x) по рекурсивному
/// соотношению:
/// H2(x) = x*H1(x) - 1*H0(x)
/// H3(x) = x*H2(x) - 2*H1(x)
/// H4(x) = x*H3(x) - 3*H2(x).
template<class T>
T hermite(size_t n, T x)
{
	if (n == 0) return 1;
	if (n == 1) return x;
	T h0 = 1;
	T h1 = x;
	T hn = 0;
	for (size_t i=2; i<=n; i++) {
		hn = x*h1 - (i-1)*h0;
		h0 = h1;
		h1 = hn;
	}
	return hn;
}

/* Calculates the T-function of Owen. Author: JC Young, Christoph Minder, C++
version by John Burkardt. */
template<class T>
T owenT(T x, T fx) {

	T fxs;
	T r[5] = {0.1477621, 0.1346334, 0.1095432, 0.0747257, 0.0333357};
	T u[5] = {0.0744372, 0.2166977, 0.3397048, 0.4325317, 0.4869533};
	T tp  = 0.159155;
	T tv1 = 1.0E-35;
	T tv2 = 15.0;
	T tv3 = 15.0;
	T tv4 = 1.0E-05;
	T x1, x2, xs;
	T r1, r2, rt;

	// Test for X near zero.
	if (abs(x) < tv1) {
		return tp*atan(fx);
	}

	// large values of abs(X) or FX near zero.
	if (tv2 < abs(x) || abs(fx) < tv1) {
		return 0.0;
	}

	// Test whether abs ( FX ) is so large that it must be truncated.
	xs = -0.5*x*x;
	x2 = fx;
	fxs = fx*fx;

	// Computation of truncation point by Newton iteration.
	if (tv3 <= log(1.0 + fxs) - xs*fxs) {
		x1 = 0.5*fx;
		fxs = 0.25f*fxs;
		for ( ; ; ) {
			rt = fxs + 1.0;
			x2 = x1 + (xs*fxs + tv3 - log(rt))/(2.0*x1*(1.0/rt - xs));
			fxs = x2 * x2;

			if (abs(x2 - x1) < tv4) {
				break;
			}
			x1 = x2;
		}
	}

	// Gaussian quadrature.
	rt = 0.0;
	for (int i=0; i<5; ++i) {
		r1 = 1.0 + fxs*pow(0.5 + u[i], 2);
		r2 = 1.0 + fxs*pow(0.5 - u[i], 2);
		rt = rt + r[i]*(exp(xs*r1)/r1 + exp(xs*r2)/r2);
	}

	return rt*x2*tp;
}


template<class T>
class Poly {
	std::valarray<T> a;

public:
	Poly(): a() {}
	Poly(size_t order): a(order+1) {}
	Poly(const std::valarray<T>& coefs): a(coefs) {}
	Poly(const Poly<T>& rhs): a(rhs.a) {}
	~Poly() {}

	Poly<T>& operator=(const Poly<T>& rhs) {
		ensure_size(rhs.a.size());
		a = rhs.a;
		return *this;
	}

	Poly<T> operator*(const Poly<T>& rhs) const {
		Poly<T> tmp(a.size() + rhs.a.size() - 1);
		for(size_t i=0; i<a.size(); ++i)
			for(size_t j=0; j<rhs.a.size(); ++j)
				tmp[i+j] += a[i]*rhs[j];
		return tmp;
	}

	inline size_t order() const {
		return a.size()-1;
	}
	inline T& operator[](size_t i) {
		return a[i];
	}
	inline T operator[](size_t i) const {
		return a[i];
	}

	template<class C>
	friend std::ostream& operator<<(std::ostream& out, const Poly<C>& rhs) {
		for (int i=rhs.order()-1; i>1; --i)
			out << std::setw(16) << std::showpos << std::right << rhs[i] << "x^" << std::noshowpos << i;
		if (rhs.order() > 0)
			out << std::setw(16) << std::showpos << std::right << rhs[1] << "x";
		if (rhs.order() >= 0)
			out << std::setw(18) << std::showpos << std::right << rhs[0];
		return out;
	}

private:
	inline void ensure_size(size_t sz) {
		if (a.size() != sz) a.resize(sz);
	}
};

template<class T>
Poly<T> hermite_poly(size_t n) {
	if (n == 0) {
		Poly<T> p(0);
		p[0] = 1;  // 1
		return p;
	}
	if (n == 1) {
		Poly<T> p(1);
		p[0] = 0;  // x
		p[1] = 1;
		return p;
	}
	// в порядке убывания степеней
	T h0[MAX_POLY_ORDER] = {1.0f};
	T h1[MAX_POLY_ORDER] = {1.0f, 0.0f};
	Poly<T> hn(n+1);
	size_t h1_size = 2;
	size_t h0_size = 1;
	size_t m = std::min(MAX_POLY_ORDER, n+1);
	for (size_t i=2; i<m; i++) {
		size_t hn_size = h1_size+1;
		for (size_t j=0; j<h1_size; j++)
			hn[j] = h1[j];
		hn[h1_size] = 0.0f;
		for (size_t j=0; j<h0_size; j++)
			hn[hn_size-h0_size+j] -= (i-1)*h0[j];
		for (size_t j=0; j<h1_size; j++)
			h0[j] = h1[j];
		h0_size = h1_size;
		for (size_t j=0; j<h1_size+1; j++)
			h1[j] = hn[j];
		h1_size++;
	}
	std::reverse(&hn[0], &hn[hn.order()]);
	return hn;
}


template<class T>
T fact(T x, T p=1)
{
	T m = 1;
	while (x > 1) {
		m *= x;
		x -= p;
	}
	return m;
}

template<class T>
inline void del(T** a, int n)
{
	for (int i=0; i<n; i++)
		delete[] a[i];
	delete[] a;
}

template<class T>
void cholesky(T** A, T* b, int n, T* x)
{
	// íèæíÿÿ òðåóãîëüíàÿ ìàòðèöà
	T** L = A;
	// ðàçëîæåíèå A=L*T (T - òðàíñïîíèðîâàííîå L)
	for (int j=0; j<n; j++) {
		T sum = 0.0;
		for (int k=0; k<j; k++) {
			sum += L[j][k]*L[j][k];
		}
		L[j][j] = sqrt(A[j][j]-sum);
		for (int i=j+1; i<n; i++) {
			sum = 0.0;
			for (int k=0; k<j; k++) {
				sum += L[i][k]*L[j][k];
			}
			L[i][j] = (A[i][j]-sum)/L[j][j];
		}
	}
	//print(L, 0, n, "L");
	// ðåøåíèå L*y=b
	T* y = x;
	for (int i=0; i<n; i++) {
		T sum = 0.0;
		for (int j=0; j<i; j++) {
			sum += L[i][j]*y[j];
		}
		y[i] = (b[i] - sum)/L[i][i];
	}
	//print(0, y, n, "y");
	// ðåøåíèå T*x=y
	for (int i=n-1; i>=0; i--) {
		T sum = 0.0;
		for (int j=i+1; j<n; j++) {
			sum += L[j][i]*x[j];
		}
		x[i] = (y[i] - sum)/L[i][i];
	}
	//print(0, x, n, "x");
	// ïðîâåðêà
	/*for (int i=0; i<n; i++) {
		T sum = 0.0;
		for (int j=0; j<n; j++) {
			sum += x[j]*L[j][i];
		}
		sum -= y[i];
		cout << setw(5) << sum << endl;
	}*/
}

// Ìåòîä íàèìåíüøèõ êâàäðàòîâ (îáùèé âèä)
// N - èñõîäíîå êîë-âî íåèçâåñòíûõ
// n - êîë-âî çíà÷èìûõ íåèçâåñòíûõ
template<class T>
void least_squares(T** P, const T* p, T** A, T* b, int n, int N)
{
	for (int k=0; k<n; k++) { // ïî íåèçâåñòíûì
		b[k] = 0.0;
		for (int j=0; j<n; j++) A[k][j] = 0.0;
		for (int i=0; i<N; i++) { // ïî ñòðîêàì
			for (int j=0; j<n; j++) {
				A[k][j] += P[i][k]*P[i][j];
//				cout << "A["<<k<<"]["<<j<<"] += P["<<i<<"]["<<k<<"]*P["<<i<<"]["<<j<<"]" << endl;
			}
			b[k] += P[i][k]*p[i];
//			cout << "b["<<k<<"] += P["<<i<<"]["<<k<<"]*p["<<i<<"]" << endl;
		}
	}
}

/// Least squares interpolation.
/// a -- interpolation coefficients
template<class T>
void interpolate(const T* x,
				 const T* y,
				 int N, T* a,
				 int n)
{
	T** A = new T*[N];
	for (int i=0; i<N; i++)
		A[i] = new T[n];

	for (int i=0; i<N; i++)
		for (int k=0; k<n; k++)
			A[i][k] = pow(x[i], k);

	T* b2 = new T[n];
	T** A2 = new T*[n];
	for (int i=0; i<n; i++)
		A2[i] = new T[n];

	least_squares(A, y, A2, b2, n, N);
	cholesky(A2, b2, n, a);

	del(A, N);
	del(A2, n);
	delete[] b2;
}

//template <class T, class F>
//inline T bisection(T a, T b, F func, T eps, uint max_iter=30) __attribute__ ((pure));

template <class T, class F>
inline T bisection(T a, T b, F func, T eps, uint max_iter=30)
{
	T c, fc;
	uint i = 0;
	do {
		c = T(0.5)*(a + b);
		fc = func(c);
		if (func(a)*fc < T(0)) b = c;
		if (func(b)*fc < T(0)) a = c;
		i++;
	} while (i<max_iter && (b-a) > eps && fabs(fc) > eps);
	return c;
}

// трехмерная интерполяция многочленом Лагранжа
// f -- массив значений функции размера size_y*size_x*size_z
// [min_*, max_*] -- пределы значений координат
// (x, y, z) -- точка, в которой аппроксимируется f(x, y, z)
template <int m, class T>
T lagrange(
	T x, T y, T z,
	const T* f, int size_x, int size_y, int size_z,
	T min_x, T max_x,
	T min_y, T max_y,
	T min_z, T max_z)
{
	T dx = (max_x-min_x)/(size_x-0);
	T dy = (max_y-min_y)/(size_y-0);
	T dz = (max_z-min_z)/(size_z-0);
	int i0 = std::max(0, (int)std::floor((x-min_x)/dx)-m/2);
	int j0 = std::max(0, (int)std::floor((y-min_y)/dy)-m/2);
	int k0 = std::max(0, (int)std::floor((z-min_z)/dz)-m/2);
	if (i0+m > size_x) i0 = size_x-m;
	if (j0+m > size_y) j0 = size_y-m;
	if (k0+m > size_z) k0 = size_z-m;
	T xx[m];
	T yy[m];
	T zz[m];
	for (int i=0; i<m; i++) {
		xx[i] = min_x+(i0+i)*dx;
		yy[i] = min_y+(j0+i)*dy;
		zz[i] = min_z+(k0+i)*dz;
	}
	//	clog << "dx dy dz = " << dx << ' ' << dy << ' ' << dz << endl;
	//	clog << "i0 j0 k0 = " << i0 << ' ' << j0 << ' ' << k0 << endl;
	//	clog << "x0 y0 z0 = " << xx[0] << ' ' << yy[0] << ' ' << zz[0]	<< endl;
	T sum = 0.0f;
	for (int i=0; i<m; i++) {
		for (int j=0; j<m; j++) {
			for (int l=0; l<m; l++) {
				T prod(1);
				for (int k=0;   k<i; k++) prod *= (x-xx[k])/(xx[i]-xx[k]);
				for (int k=i+1; k<m; k++) prod *= (x-xx[k])/(xx[i]-xx[k]);
				for (int k=0;   k<j; k++) prod *= (y-yy[k])/(yy[j]-yy[k]);
				for (int k=j+1; k<m; k++) prod *= (y-yy[k])/(yy[j]-yy[k]);
				for (int k=0;   k<l; k++) prod *= (z-zz[k])/(zz[l]-zz[k]);
				for (int k=l+1; k<m; k++) prod *= (z-zz[k])/(zz[l]-zz[k]);
				sum += f[(i0+i)*size_y*size_z + (j0+j)*size_z + (k0+l)]*prod;
			}
		}
	}
	return sum;
}

template <int m, class T>
T lagrange(T x, T y, T z,
		   const T* f, const Domain<T>& dom)
{
	return lagrange<m>(x, y, z, f,
					   dom.count()[0], dom.count()[1], dom.count()[2],
					   dom.min()[0], dom.max()[0],
					   dom.min()[1], dom.max()[1],
					   dom.min()[2], dom.max()[2]);
}

}

#endif // APPS_AUTOREG_EMATH_HH
