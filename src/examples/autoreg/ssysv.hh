#ifndef EXAMPLES_AUTOREG_SSYSV_HH
#define EXAMPLES_AUTOREG_SSYSV_HH

#include "vector_n.hh"
#include <cmath>

namespace autoreg {

	/// Solve linear system of equations A*x=b with Cholesky decomposition.
	template<class T>
	void
	cholesky(T* A, T* b, int n) {
		Index<2> idx(size2(n, n));
		// A=L*T (T - transposed L)
		for (int j=0; j<n; j++) {
			T sum = 0;
			for (int k=0; k<j; k++) {
				sum += A[idx(j,k)]*A[idx(j,k)];
			}
			A[idx(j,j)] = std::sqrt(A[idx(j,j)]-sum);
			for (int i=j+1; i<n; i++) {
				sum = 0;
				for (int k=0; k<j; k++) {
					sum += A[idx(i,k)]*A[idx(j,k)];
				}
				A[idx(i,j)] = (A[idx(i,j)]-sum)/A[idx(j,j)];
			}
		}
		// solve L*y=b
		T* y = b;
		for (int i=0; i<n; i++) {
			T sum = 0;
			for (int j=0; j<i; j++) {
				sum += A[idx(i,j)]*y[j];
			}
			y[i] = (b[i] - sum)/A[idx(i,i)];
		}
		// solve T*b=y
		for (int i=n-1; i>=0; i--) {
			T sum = 0;
			for (int j=i+1; j<n; j++) {
				sum += A[idx(j,i)]*b[j];
			}
			b[i] = (y[i] - sum)/A[idx(i,i)];
		}
		//print(0, b, n, "b");
		/*for (int i=0; i<n; i++) {
		    T sum = 0.0;
		    for (int j=0; j<n; j++) {
		        sum += b[j]*A[idx(j,i)];
		    }
		    sum -= y[i];
		    cout << setw(5) << sum << endl;
		   }*/
	}

}

#endif // vim:filetype=cpp
