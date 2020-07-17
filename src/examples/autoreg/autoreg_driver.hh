#ifndef EXAMPLES_AUTOREG_AUTOREG_DRIVER_HH
#define EXAMPLES_AUTOREG_AUTOREG_DRIVER_HH

#include <subordination/core/kernel_buffer.hh>

#include <autoreg/autoreg.hh>
#include <autoreg/autoreg_driver.hh>
#include <autoreg/domain.hh>
#include <autoreg/mapreduce.hh>
#include <autoreg/ssysv.hh>
#include <autoreg/valarray_ext.hh>

namespace autoreg {

    template<class T>
    class Autoreg_model: public sbn::kernel {

    private:
        size3 zsize{2000, 32, 32};
        Vector<T,3> zdelta{1,1,1};
        size3 acf_size{10, 10, 10};
        Vector<T,3> acf_delta;
        size3 fsize;

        int interval = 0;
        size3 zsize2;

        std::valarray<T> acf_model;
        std::valarray<T> acf_pure;
        std::valarray<T> ar_coefs;

        bool _homogeneous = true;

        /// ACF parameters.
        T _alpha{0.05}, _beta{0.8}, _gamma{1.0};

        std::string _filename;

    public:

        inline explicit Autoreg_model():
        acf_delta(zdelta), fsize(acf_size), zsize2(zsize*T{1.4}),
        acf_model(acf_size.product()), acf_pure(acf_size.product()),
        ar_coefs(fsize.product()) {
            validate_parameters();
        }

        void act() override;
        void react(sbn::kernel_ptr&& child) override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const Domain<T,3> domain() const noexcept { return Domain<T,3>(zdelta, zsize); }

        template<class V>
        friend std::istream&
        operator>>(std::istream& in, Autoreg_model<V>& model);

    private:

        inline T size_factor() const noexcept { return T(zsize2[0]) / T(zsize[0]); }
        inline int part_size() const noexcept { return floor(2 * fsize[0]); }

        void read_input_file();
        void validate_parameters();

    };

    template <class T> std::istream&
    operator>>(std::istream& in, Autoreg_model<T>& m);

}

#endif // vim:filetype=cpp
