#include <autoreg/acf_generator.hh>
#include <autoreg/mapreduce.hh>

template <class T> void
autoreg::ACF_generator<T>::act() {
    int bs = 2;
    int n = _acf_size[0];
    auto identity = [](int){};
    sbn::upstream(this, sbn::mapreduce([this](int t) {
        const Index<3> id(_acf_size);
        int x1 = _acf_size[1];
        int y1 = _acf_size[2];
        for (int x=0; x<x1; x++) {
            for (int y=0; y<y1; y++) {
                const T k1 = t*_delta[0] + x*_delta[1] + y*_delta[2];
                _acf[id(t, x, y)] = _gamma*exp(-_alpha*k1)*
                                           cos(_beta*x*_delta[1])*
                                           cos(_beta*t*_delta[0]);
                //_acf[id(t, x, y)] = _gamma*exp(-_alpha*k1)*cos(_beta*t*_delta[0])*cos(_beta*x*_delta[1])*cos(_beta*y*_delta[2]);
                //_acf[id(t, x, y)] = _gamma*exp(-_alpha*k1)*cos(-_beta*t*_delta[0] + _beta*x*_delta[1] + _beta*y*_delta[2]);
            }
        }
    }, identity, 0, n, bs));
}

template <class T> void
autoreg::ACF_generator<T>::react(sbn::kernel_ptr&&) {
    sbn::commit<sbn::Local>(std::move(this_ptr()));
}

template class autoreg::ACF_generator<AUTOREG_REAL_TYPE>;
