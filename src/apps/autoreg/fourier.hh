#ifndef APPS_AUTOREG_FOURIER_HH
#define APPS_AUTOREG_FOURIER_HH

/*
 * Original code by Magmai Kai Holmlor,
 * 19.03.2001,
 * http://www.gamedev.net/page/resources/_/technical/math-and-physics/fast-fourier-transforms-r1349.
 *
 */

template<class T>
std::complex<T> fft_exp(T theta) {
	return std::complex<T>(std::cos(theta), std::sin(theta));
}

template<class T>
struct Forward {
	std::complex<T> exponent(T theta) const { return fft_exp(-theta); }
	template<class Complex_array>
	void normalise(Complex_array, int32_t) const {}
};

template<class T>
struct Backward {
	std::complex<T> exponent(T theta) const { return fft_exp(theta); }
	template<class Complex_array>
	void normalise(Complex_array out, int32_t N) const {
		for (int32_t i=0; i<N; ++i) {
			out[i] /= N;
		}
	}
};

template<class T>
struct Backward_2 {
	std::complex<T> exponent(T theta) const { return fft_exp(theta); }
	template<class Complex_array>
	void normalise(Complex_array, int32_t) const {}
};


typedef uint32_t Dimension;

template<class T, class Direction = Forward<T>, Dimension D = 1>
struct Fourier_transform;

template<class T, class Direction>
struct Fourier_transform<T, Direction, 1> {

	explicit Fourier_transform(int32_t N):
		_size(N), _reverse_table(), _exponent(MAX_BITS)
	{
		int32_t log2N = fft_log2(N);
		init_reverse_table(log2N);
		init_exponents(log2N);
	}

	template<class Real_array, class Complex_array>
	void operator()(const Real_array in, Complex_array& out) const {
		transform<const Real_array&, Complex_array&>(in, out);
	}

	void operator()(const T* in, std::complex<T>* out) const {
		transform<const T*, std::complex<T>*>(in, out);
	}

	void operator()(const std::complex<T>* in, std::complex<T>* out) const {
		transform<const std::complex<T>*, std::complex<T>*>(in, out);
	}

	int32_t size() const { return _size; }
	int32_t size() { return _size; }

private:
//	void operator()(const T* in, std::complex<T>* out, int32_t N, bool inverse) {

	template<class Real_array, class Complex_array>
	void transform(Real_array in, Complex_array out) const {

		int32_t N = _size;
		int32_t log2N = 0;
		for (int32_t i=N; i>1; i>>=1)
			log2N++;
		if (log2N > MAX_BITS) return; //Too Big
		int32_t n = 1;
		n <<= log2N;
		if (N != n) //Not a power of 2
			return;

		for (int32_t i=0; i<N; i++) {
			out[i] = in[_reverse_table[i]];  //Copy & Sort
		}

		int32_t widx = 0;
		int32_t subFFT = N;
		//m=1 is accomplished by the BRS
		for (int32_t m=2; m<=N; m<<=1) //Number of iterations required to "unrecurse" the subFFT's
		{
			subFFT >>= 1; //Number of sub-FFT's currently in the list, N/2,N/4,.,2,1 done!
			++widx;
			int32_t halfm = m >> 1;
			for (int32_t j=0; j<subFFT; ++j) {
				for (int32_t k=0; k<halfm; ++k) {

					int32_t i = k + j*m;
					int32_t l = i + halfm;

					// TODO: check this
					std::complex<T> Fi = out[i];
					std::complex<T> exp = _exponent[widx][k];
					out[i] += exp * out[l];
					out[l] = Fi - exp * out[l];
				}
			}
		}

		// Normalize for inverse transform
		dir.normalise(out, N);
	}

#pragma GCC push_options
#pragma GCC optimize (0)
	static int32_t reverse_bits(int32_t i, int32_t log2N) {
		//i contains log2N bits of data
		//the log2N bits of data will be reserved and returned
		//i.e.  00001011b, 4 will return 00001101b
		//      00001001b, 5 will return 00010010b

		//It was felt that this was much easier than using lots of masks & shifts in C/C++
		int32_t x;
		asm("\
			xor %%ebx, %%ebx; \
	L%=:    shrl $1, %%eax; \
			rcll $1, %%ebx; \
			loop L%=; \
		"
		: "=b" (x)
		: "a" (i), "c" (log2N));
		return x;
	}
#pragma GCC pop_options

	// Create Bit Reverse List of order log2N
	void init_reverse_table(int32_t log2N) {
		int32_t list = 1 << log2N;
		_reverse_table.resize(list);
		for (int32_t i=0; i<list; ++i)
			_reverse_table[i] = reverse_bits(i, log2N);
	}

	// Pre-calculate _exponent's
	void init_exponents(int32_t log2N) {
		for (int32_t i=log2N; i>0; --i) {
			int32_t n = 1 << i;
			int32_t halfn = n >> 1;
			_exponent[i].resize(halfn);
			for (int32_t j=0; j<halfn; j++)
				_exponent[i][j] = dir.exponent(_2PI * j / n);
		}
	}

	static int32_t fft_log2(int32_t N) {
		int32_t log2N = 0;
		for (int32_t i=N; i>1; i>>=1)
			log2N++;
		return log2N;
	}


	int32_t _size;
	std::valarray<int32_t> _reverse_table;
	std::valarray<std::valarray<std::complex<T>>> _exponent;

	Direction dir;

	static const int MAX_BITS = sizeof(int32_t)*8;
	static const T _2PI;
};

template<class T, class P>
const T Fourier_transform<T, P, 1>::_2PI = T(2) * std::acos(T(-1));

template<class T, class Array>
struct Slice {

	Slice(const Array& arr, int32_t slice):
		_array(arr), _slice(slice) {}

	const T& operator[](int32_t i) const { return _array[i][_slice]; }
	T& operator[](int32_t i) { return _array[i][_slice]; }

private:
	const Array& _array;
	int32_t _slice;
};

template<class T, class Direction>
struct Fourier_transform<T, Direction, 2> {

	Fourier_transform(int32_t N_1, int32_t N_2):
		_transform_x(N_1), _transform_y(N_2), _tmp_out(N_1) {}

	template<class Real_array, class Complex_array>
	void operator()(const Real_array& in, Complex_array& out) {
		transform<const Real_array&, Complex_array&>(in, out);
	}

private:

	template<class Real_array, class Complex_array>
	void transform(Real_array in, Complex_array out) {

		int32_t size_x = _transform_x.size();
		int32_t size_y = _transform_y.size();

		// TODO: optimise when size_x == size_y
		for (int32_t i=0; i<size_x; ++i) {
			_transform_y(&in[i][0], &out[i][0]);
//			_transform_y(in[i], out[i]);
		}
//		std::valarray<std::complex<T>> tmp_in(size_x);
//		std::valarray<std::complex<T>> tmp_out(size_x);
		for (int32_t j=0; j<size_y; ++j) {
//			for (int32_t i=0; i<size_x; ++i) {
//				tmp_in[i] = out[i][j];
//			}
//			_transform_x(&tmp_in[0], &tmp_out[0]);
			Slice<std::complex<T>, Complex_array> slice(out, j);
			_transform_x(slice, _tmp_out);
			for (int32_t i=0; i<size_x; ++i) {
				out[i][j] = _tmp_out[i];
			}
		}
	}

	Fourier_transform<T, Direction, 1> _transform_x, _transform_y;
	std::valarray<std::complex<T>> _tmp_out;
};

#endif // APPS_AUTOREG_FOURIER_HH
