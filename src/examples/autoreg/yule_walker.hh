#ifndef EXAMPLES_AUTOREG_YULE_WALKER_HH
#define EXAMPLES_AUTOREG_YULE_WALKER_HH

#include <valarray>
#include <sstream>

#include <autoreg/api.hh>
#include <autoreg/vector_n.hh>

namespace autoreg {

    template<class T>
    struct Autoreg_coefs: public sbn::kernel {

    private:
        const std::valarray<T>& acf_model;
        const size3& acf_size;
        std::valarray<T>& ar_coefs;
        std::valarray<T> a;
        std::valarray<T> b;
        int state = 0;
        int count = 0;

    public:
        Autoreg_coefs(const std::valarray<T>& acf_model_,
                      const size3& acf_size_,
                      std::valarray<T>& ar_coefs_):
            acf_model(acf_model_), acf_size(acf_size_), ar_coefs(ar_coefs_),
            a((ar_coefs.size()-1)*(ar_coefs.size()-1)), b(ar_coefs.size()-1) {}

        void act() override;
        void react(sbn::kernel_ptr&&) override;

    private:
        void solve();

    };


}

#endif // vim:filetype=cpp
