#include <autoreg/mapreduce.hh>
#include <autoreg/ssysv.hh>
#include <autoreg/yule_walker.hh>

namespace  {

    inline int abs_sub(int a, int b) {
        return (a > b) ? a-b : b-a;
    }

    template<class T>
    inline bool is_stationary(std::valarray<T>& ar_coefs) {
        int n = ar_coefs.size();
        for (int i=0; i<n; ++i)
            if (std::abs(ar_coefs[i]) > T(1))
                return false;
        return true;
    }

}

template <class T>
void autoreg::Autoreg_coefs<T>::solve() {
    int m = ar_coefs.size()-1;
    cholesky(&a[0], &b[0], m);
    std::copy(&b[0], &b[m], &ar_coefs[1]);
    ar_coefs[0] = -1;
    if (!is_stationary(ar_coefs)) {
        std::stringstream msg;
        msg << "Process is not stationary: |f(i)| > 1\n";
//			int n = ar_coefs.size();
        Index<3> idx(acf_size);
        for (size_t i=0; i<acf_size[0]; ++i)
            for (size_t j=0; j<acf_size[1]; ++j)
                for (size_t k=0; k<acf_size[2]; ++k)
                    if (std::abs(ar_coefs[idx(i, j, k)]) > T(1))
                        msg << "ar_coefs[" << i << ',' << j << ',' << k << "] = " << ar_coefs[idx(i, j, k)] << '\n';
        throw std::runtime_error(msg.str());
    }
}


template <class T>
void autoreg::Autoreg_coefs<T>::act() {
    // generate lhs and rhs
    int n = acf_model.size() - 1;
    int block_size = 16*4;
    int m = b.size();
    auto identity = [](int){};
    sbn::upstream(this, sbn::mapreduce([this](int i) {
        const uint32_t n = acf_model.size()-1;
        const Index<3> id(acf_size);
        const Index<2> ida(size2(n, n));
        for (uint32_t j=0; j<n; j++) {
            // casting to signed type ptrdiff_t
            int i2 = id(abs_sub(id.t(i+1), id.t(j+1)),
                        abs_sub(id.x(i+1), id.x(j+1)),
                        abs_sub(id.y(i+1), id.y(j+1)) );
            int i1 = i*n + j; //ida(i, j);
            a[i1] = acf_model[i2];
        }
    }, identity, 0, n, block_size));
    sbn::upstream(this, sbn::mapreduce([this](int i) {
        const Index<3> id(acf_size);
        b[i] = acf_model[id( id.t(i+1), id.x(i+1), id.y(i+1) )];
    }, identity, 0, m, block_size));
}

template <class T>
void autoreg::Autoreg_coefs<T>::react(sbn::kernel_ptr&&) {
    switch (state) {
        case 0:
            ++state;
            break;
        case 1:
            solve();
            sbn::commit<sbn::Local>(std::move(this_ptr()));
            break;
    }
}

template class autoreg::Autoreg_coefs<AUTOREG_REAL_TYPE>;
