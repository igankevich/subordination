#ifndef EXAMPLES_AUTOREG_ACF_GENERATOR_HH
#define EXAMPLES_AUTOREG_ACF_GENERATOR_HH

#include <valarray>

#include <subordination/api.hh>

#include <autoreg/vector_n.hh>

namespace autoreg {

    template <class T>
    class ACF_generator: public sbn::kernel {

    private:
        T _alpha;
        T _beta;
        T _gamma;
        const Vector<T,3>& _delta;
        const size3& _acf_size;
        std::valarray<T>& _acf;

    public:

        inline ACF_generator(T alpha, T beta, T gamm,
                             const Vector<T,3>& delta,
                             const size3& acf_size,
                             std::valarray<T>& acf):
        _alpha(alpha), _beta(beta), _gamma(gamm),
        _delta(delta), _acf_size(acf_size), _acf(acf) {}

        void act() override;
        void react(sbn::kernel_ptr&&) override;

    };

}

#endif // vim:filetype=cpp
