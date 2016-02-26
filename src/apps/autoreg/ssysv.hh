#ifndef APPS_AUTOREG_SSYSV_HH
#define APPS_AUTOREG_SSYSV_HH

#ifdef USE_MKL
#include "mkl.h"
//typedef long long Lapack_int;
typedef int Lapack_int;
#else
extern "C" void ssysv_(char*, int*, int*, float*, int*, int*, float*, int*, float*, int*, int*);
extern "C" void zsysvx_(char* FACT, char* UPLO, int* N, int* NRHS, std::complex<double>* A, int* LDA, std::complex<double>* AF, int* LDAF, int* IPIV, std::complex<double>* B, int* LDB, std::complex<double>* X, int* LDX, std::complex<double>* RCOND, std::complex<double>* ferr, std::complex<double>* berr, std::complex<double>* work, int* lwork, std::complex<double>* rwork, int* info);
extern "C" void dsysv_(char*, int*, int*, double*, int*, int*, double*, int*, double*, int*, int*);
typedef int Lapack_int;
#endif

template<class T>
void sysv(char type, Lapack_int m, Lapack_int nrhs, T* a, Lapack_int lda, T* b, Lapack_int ldb, Lapack_int* info);

template<>
void sysv<float>(char type, Lapack_int m, Lapack_int nrhs, float* a, Lapack_int lda, float* b, Lapack_int ldb, Lapack_int* info)
{
	Lapack_int lwork = m;
	std::valarray<float> work(lwork);
	std::valarray<Lapack_int> ipiv(m);
	ssysv_(&type, &m, &nrhs, a, &lda, &ipiv[0], b, &ldb, &work[0], &lwork, info);
}

template<>
void sysv<double>(char type, Lapack_int m, Lapack_int nrhs, double* a, Lapack_int lda, double* b, Lapack_int ldb, Lapack_int* info)
{
	Lapack_int lwork = m;
	std::valarray<double> work(lwork);
	std::valarray<Lapack_int> ipiv(m);
	dsysv_(&type, &m, &nrhs, a, &lda, &ipiv[0], b, &ldb, &work[0], &lwork, info);
}

template<class T>
void zsysvx(char uplo, T* a, T* b, int n, T* x) {
	char fact = 'N';
	int nrhs = 1;
	int ldb = n;
	std::valarray<T> af(n*n);
	std::valarray<int> ipiv(n);
	int ldx = n;
	T rcond = 0;
	T ferr = 0;
	T berr = 0;
	int lwork = 3*n;
	std::valarray<T> work(lwork);
	std::valarray<T> rwork(n);
	int info = 0;
	zsysvx_(&fact, &uplo, &n, &nrhs, a, &n, &af[0], &n, &ipiv[0], b, &ldb, x, &ldx, &rcond, &ferr, &berr, &work[0], &lwork, &rwork[0], &info);

	std::cout << "info = " << info << std::endl;
	std::cout << "rcond = " << rcond << std::endl;
	std::cout << "ferr = " << ferr << std::endl;
	std::cout << "berr = " << berr << std::endl;
}

#endif // APPS_AUTOREG_SSYSV_HH
