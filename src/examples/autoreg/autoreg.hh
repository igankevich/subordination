#ifndef EXAMPLES_AUTOREG_AUTOREG_HH
#define EXAMPLES_AUTOREG_AUTOREG_HH

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <valarray>

#include <unistdx/base/make_object>
#include <unistdx/ipc/process>

#include <autoreg/domain.hh>
#include <autoreg/mapreduce.hh>
#include <autoreg/valarray_ext.hh>
#include <autoreg/vector_n.hh>
#include <autoreg/yule_walker.hh>

#if defined(AUTOREG_MPI)
#include <autoreg/mpi.hh>
#endif

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
        enum class State: uint8_t {Submitted, Completed, Initial};
        using clock_type = std::chrono::system_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

    private:
        size3 _begin_index;
        size3 _offset;
        size3 _end_index;
        uint32_t _index = 0;
        State _state = State::Initial;
        std::array<time_point,2> _time_points;

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
        inline void state(State rhs) noexcept {
            this->_state = rhs;
            this->_time_points[int(rhs)] = clock_type::now();
        }
        inline duration total_time() const noexcept {
            return this->_time_points[1] - this->_time_points[0];
        }
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
        T _white_noise_variance;
        uint64_t _seed = 0;

    public:
        Part_generator() = default;

        inline explicit
        Part_generator(const Part& part, const size3& phi_size, const std::valarray<T>& phi,
                       std::valarray<T>&& zeta, T white_noise_variance, uint64_t seed):
        _part(part), _phi_size(phi_size), _phi(phi), _zeta(std::move(zeta)),
        _white_noise_variance(white_noise_variance), _seed(seed) {}

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
        #if defined(AUTOREG_MPI)
        class Request {
        private:
            mpi::request _request;
            sbn::kernel_buffer _buffer;
        public:
            inline Request(mpi::request&& r, sbn::kernel_buffer&& b) noexcept:
            _request(std::move(r)), _buffer(std::move(b)) {}
            Request() = default;
            ~Request() = default;
            Request(const Request&) = delete;
            Request& operator=(const Request&) = delete;
            inline Request(Request&& rhs) noexcept:
            _request(std::move(rhs._request)), _buffer(std::move(rhs._buffer)) {}
            inline Request& operator=(Request&& rhs) noexcept {
                swap(this->_request, rhs._request);
                swap(this->_buffer, rhs._buffer);
                return *this;
            }
            inline mpi::request& request() noexcept { return this->_request; }
            inline sbn::kernel_buffer& buffer() noexcept { return this->_buffer; }
        };
        #endif

    private:
        std::valarray<T> _phi;
        size3 _phi_size;
        T _white_noise_variance;
        size3 _zeta_size_full;
        size3 _zeta_size;
        size3 _part_size;
        std::vector<Part> _parts;
        std::valarray<T> _zeta;
        uint64_t _seed = 0;
        std::array<time_point,4> _time_points;
        #if defined(AUTOREG_MPI)
        sbn::kernel_buffer _buffer{4096*1096};
        sbn::kernel_buffer _output_buffer{4096*1096};
        std::vector<Request> _requests;
        int _subordinate = 0;
        #endif
        bool _verify = true;

    public:

        Wave_surface_generator() = default;

        Wave_surface_generator(const std::valarray<T>& phi,
                               const size3& phi_size,
                               const T white_noise_variance,
                               const size3& zsize2_,
                               const size3& zeta_size,
                               const size3& part_size,
                               bool verify,
                               size_t buffer_size):
        _phi(phi), _phi_size(phi_size), _white_noise_variance(white_noise_variance),
        _zeta_size_full(zsize2_), _zeta_size(zeta_size), _part_size(part_size),
        #if defined(AUTOREG_MPI)
        _output_buffer(buffer_size),
        #endif
        _verify(verify) {}

        void act() override;
        void react(sbn::kernel_ptr&& child) override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const std::array<time_point,4>& time_points() const noexcept {
            return this->_time_points;
        }

        inline duration verification_duration() const noexcept {
            using namespace std::chrono;
            const auto& t = this->_time_points;
            return t[3]-t[2];
        }

    private:
        void generate_white_noise(std::valarray<T>& zeta) const;
        void generate_surface(std::valarray<T>& zeta) const;
        void trim_surface(std::valarray<T>& zeta) const;
        void generate_parts();
        bool push_kernels();
        void verify();
        void write_zeta(const std::valarray<T>& zeta, int time_slice) const;
        #if defined(AUTOREG_MPI)
        void remove_completed_requests();
        #endif

    };

}

#endif // vim:filetype=cpp
