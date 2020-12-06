#ifndef EXAMPLES_CPP_AUTOREG_AUTOREG_DRIVER_HH
#define EXAMPLES_CPP_AUTOREG_AUTOREG_DRIVER_HH

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
        using clock_type = std::chrono::system_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

    private:
        size3 zsize{1000, 32, 32};
        Vector<T,3> zdelta{1,1,1};
        size3 acf_size{10, 10, 10};
        Vector<T,3> acf_delta;
        size3 _phi_size;
        size3 _part_size;
        size3 _zeta_padding;
        size3 zsize2;
        size_t _buffer_size = 4096*1024;

        std::valarray<T> acf_model;
        std::valarray<T> acf_pure;
        std::valarray<T> ar_coefs;

        bool _verify = true;
        bool _homogeneous = true;

        /// ACF parameters.
        T _alpha{0.05}, _beta{0.8}, _gamma{1.0};

        std::string _filename;
        time_point _time_points[2];

    public:

        inline explicit Autoreg_model():
        acf_delta(zdelta), _phi_size(acf_size),
        _part_size(_phi_size*2), _zeta_padding(_phi_size), zsize2(zsize + _zeta_padding),
        acf_model(acf_size.product()), acf_pure(acf_size.product()),
        ar_coefs(_phi_size.product()) {
            validate_parameters();
        }

        void act() override;
        void react(sbn::kernel_ptr&& child) override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const Domain<T,3> domain() const noexcept { return Domain<T,3>(zdelta, zsize); }
        inline const std::string& filename() const noexcept { return this->_filename; }
        inline void filename(const std::string& rhs) noexcept { this->_filename = rhs; }
        inline const size3& part_size() const noexcept { return this->_part_size; }

        void num_coefficients(const size3& rhs);
        inline const size3& num_coefficients() const noexcept { return this->_phi_size; }
        void zeta_padding(const size3& rhs);

        template<class V>
        friend std::istream&
        operator>>(std::istream& in, Autoreg_model<V>& model);

    private:

        inline T size_factor() const noexcept { return T(zsize2[0]) / T(zsize[0]); }

        void read_input_file();
        void validate_parameters();

    };

    template <class T> std::istream&
    operator>>(std::istream& in, Autoreg_model<T>& m);

}

#endif // vim:filetype=cpp
