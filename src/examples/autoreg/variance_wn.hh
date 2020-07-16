#ifndef EXAMPLES_AUTOREG_VARIANCE_WN_HH
#define EXAMPLES_AUTOREG_VARIANCE_WN_HH

#include <valarray>

#include <subordination/api.hh>

namespace autoreg {

    template <class T>
    class Variance_WN: public sbn::kernel {

    private:
        const std::valarray<T>& ar_coefs;
        const std::valarray<T>& acf;
        T _sum{};

    public:

        inline Variance_WN(const std::valarray<T>& ar_coefs_, const std::valarray<T>& acf_):
        ar_coefs(ar_coefs_), acf(acf_) {}

        void act() override;
        void react(sbn::kernel_ptr&&) override;

        inline T get_sum() const noexcept { return _sum; }

    };

}


#endif // vim:filetype=cpp
