#ifndef EXAMPLES_AUTOREG_AUTOREG_HH
#define EXAMPLES_AUTOREG_AUTOREG_HH

#include <chrono>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <valarray>

#include <unistdx/base/make_object>

#include <autoreg/domain.hh>
#include <autoreg/mapreduce.hh>
#include <autoreg/valarray_ext.hh>
#include <autoreg/vector_n.hh>
#include <autoreg/yule_walker.hh>

namespace std {

    template<class T>
    sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::valarray<T>& rhs) {
        out << uint32_t(rhs.size());
        for (size_t i=0; i<rhs.size(); ++i) {
            out << rhs[i];
        }
        return out;
    }

    template<class T>
    sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::valarray<T>& rhs) {
        uint32_t n = 0;
        in >> n;
        rhs.resize(n);
        for (size_t i=0; i<rhs.size(); ++i) {
            in >> rhs[i];
        }
        return in;
    }

}

namespace autoreg {

    class Part {

    public:
        enum class State: uint8_t {Initial, Submitted, Completed};

    private:
        size3 _begin_index;
        size3 _offset;
        size3 _end_index;
        uint32_t _index = 0;
        State _state = State::Initial;

    public:

        inline explicit Part(size3 begin, size3 offset, size3 end, uint32_t index):
        _begin_index(begin), _offset(offset), _end_index(end), _index(index) {}

        Part() = default;
        ~Part() = default;
        Part(const Part&) = default;
        Part& operator=(const Part&) = default;
        Part(Part&&) = default;
        Part& operator=(Part&&) = default;

        inline const size3& begin_index() const noexcept { return this->_begin_index; }
        inline const size3& offset() const noexcept { return this->_offset; }
        inline const size3& end_index() const noexcept { return this->_end_index; }
        inline uint32_t index() const noexcept { return this->_index; }
        inline State state() const noexcept { return this->_state; }
        inline void state(State rhs) noexcept { this->_state = rhs; }
        inline bool completed() const noexcept { return this->_state == State::Completed; }
        inline bool submitted() const noexcept { return this->_state == State::Submitted; }

        inline std::gslice slice_in(const size3& zsize2) const {
            const auto size = end_index() - begin_index();
            Index<3> index(zsize2);
            return std::gslice{
                static_cast<size_t>(index(begin_index())),
                {size[0], size[1], size[2]},
                {zsize2[1]*zsize2[2], zsize2[2], 1}
            };
        }

        inline std::gslice slice_out(const size3& zsize2) const {
            const auto begin = begin_index() + offset();
            const auto size = end_index() - begin;
            Index<3> index(zsize2);
            return std::gslice{
                static_cast<size_t>(index(begin)),
                {size[0], size[1], size[2]},
                {zsize2[1]*zsize2[2], zsize2[2], 1}
            };
        }

        inline std::gslice slice_out() const {
            const auto n = end_index() - begin_index();
            const auto size = n - offset();
            Index<3> index(n);
            return std::gslice{
                static_cast<size_t>(index(offset())),
                {size[0], size[1], size[2]},
                {n[1]*n[2], n[2], 1}
            };
        }

        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

    };

    inline sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const Part& rhs) {
        rhs.write(out);
        return out;
    }

    inline sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, Part& rhs) {
        rhs.read(in);
        return in;
    }

    template <class T>
    class Part_generator: public sbn::kernel {

    private:
        Part _part;
        size3 _phi_size;
        std::valarray<T> _phi;
        std::valarray<T> _zeta;

    public:
        Part_generator() = default;

        inline explicit
        Part_generator(const Part& part, const size3& phi_size, const std::valarray<T>& phi,
                       std::valarray<T>&& zeta):
        _part(part), _phi_size(phi_size), _phi(phi), _zeta(std::move(zeta)) {}

        void act() override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const Part& part() const noexcept { return this->_part; }
        inline size3 zeta_size() const { return part().end_index() - part().begin_index(); }
        inline const std::valarray<T>& zeta() const noexcept { return this->_zeta; }

    };

    template <class T>
    class Wave_surface_generator: public sbn::kernel {

    private:
        using clock_type = std::chrono::system_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

    private:
        std::valarray<T> _phi;
        size3 _phi_size;
        T _white_noise_variance;
        size3 zsize2;
        size3 zsize;
        std::vector<Part> _parts;
        std::valarray<T> _zeta;
        uint64_t _seed = 0;
        time_point _time_points[4];

    public:

        Wave_surface_generator() = default;

        Wave_surface_generator(const std::valarray<T>& phi,
                               const size3& fsize,
                               const T white_noise_variance,
                               const size3& zsize2_,
                               const size3& zsize_):
        _phi(phi), _phi_size(fsize), _white_noise_variance(white_noise_variance),
        zsize2(zsize2_), zsize(zsize_) {}

        void act() override;
        void react(sbn::kernel_ptr&& child) override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    private:
        void generate_white_noise(std::valarray<T>& zeta) const;
        void generate_surface(std::valarray<T>& zeta) const;
        void trim_surface(std::valarray<T>& zeta) const;
        void generate_parts();
        void push_kernels();
        void verify();
        void write_zeta(const std::valarray<T>& zeta, int time_slice) const;

    };

}

#endif // vim:filetype=cpp
