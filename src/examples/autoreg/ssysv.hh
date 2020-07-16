#ifndef EXAMPLES_AUTOREG_SSYSV_HH
#define EXAMPLES_AUTOREG_SSYSV_HH

#include <cstdint>

namespace autoreg {

    /// Solve linear system of equations A*x=b with Cholesky decomposition.
    template <class T> void
    cholesky(T* A, T* b, std::uint32_t N);

}

#endif // vim:filetype=cpp
