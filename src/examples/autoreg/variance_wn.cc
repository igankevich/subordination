#include <autoreg/mapreduce.hh>
#include <autoreg/variance_wn.hh>

template <class T> void
autoreg::Variance_WN<T>::act() {
    const int bs = 64;
    const int n = ar_coefs.size();
    sbn::upstream(this, sbn::mapreduce([](int) {}, [this](int i){
        _sum += ar_coefs[i]*acf[i];
    }, 0, n, bs));
}

template <class T> void
autoreg::Variance_WN<T>::react(sbn::kernel_ptr&&) {
    //_sum = acf[0] - _sum;
    _sum = -_sum;
    sbn::commit(std::move(this_ptr()));
}

template class autoreg::Variance_WN<AUTOREG_REAL_TYPE>;
