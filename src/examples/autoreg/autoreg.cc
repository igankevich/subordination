#include <random>

#include <autoreg/autoreg.hh>
#include <autoreg/mpi.hh>

namespace  {
    constexpr const auto tag_kernel = 1;
    constexpr const auto tag_kernel_downstream = 2;
    constexpr const auto tag_terminate = 3;
}

void autoreg::Part::write(sbn::kernel_buffer& out) const {
    out << this->_begin_index << this->_offset << this->_end_index
        << this->_index << this->_state;
}

void autoreg::Part::read(sbn::kernel_buffer& in) {
    in >> this->_begin_index >> this->_offset >> this->_end_index
       >> this->_index >> this->_state;
}

template <class T> void
autoreg::Wave_surface_generator<T>::generate_white_noise(std::valarray<T>& zeta) const {
    const auto variance = this->_white_noise_variance;
    if (variance < T{}) {
        throw std::runtime_error("white noise variance is less than zero");
    }
    if (std::isnan(variance)) { throw std::runtime_error("white noise variance is NaN"); }
    zeta.resize(this->_zeta_size_full.product());
    Index<3> zeta_index(this->_zeta_size_full);
    for (const auto& part : this->_parts) {
        std::mt19937 generator;
        generator.seed(this->_seed);
        std::normal_distribution<T> normal(T{}, std::sqrt(variance));
        const Vector<int,3> begin = part.begin_index() + part.offset();
        const Vector<int,3> end = part.end_index();
        for (int i=begin[0]; i<end[0]; ++i) {
            for (int j=begin[1]; j<end[1]; ++j) {
                for (int k=begin[2]; k<end[2]; ++k) {
                    zeta[zeta_index(i,j,k)] = normal(generator);
                }
            }
        }
    }
    for (const auto& z : zeta) {
        if (std::isnan(z)) {
            throw std::runtime_error("white noise generator produced some NaNs");
        }
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::generate_surface(std::valarray<T>& zeta) const {
    auto& phi = this->_phi;
    Vector<int,3> size = this->_zeta_size_full;
    Index<3> zeta_index(size);
    Index<3> phi_index(this->_phi_size);
    for (int i=0; i<size[0]; ++i) {
        for (int j=0; j<size[1]; ++j) {
            for (int k=0; k<size[2]; ++k) {
                const Vector<int,3> lag = min(size3{i+1,j+1,k+1}, this->_phi_size);
                T sum{};
                for (int l=0; l<lag[0]; ++l) {
                    for (int m=0; m<lag[1]; ++m) {
                        for (int n=0; n<lag[2]; ++n) {
                            sum += phi[phi_index(l,m,n)] * zeta[zeta_index(i-l,j-m,k-n)];
                        }
                    }
                }
                zeta[zeta_index(i,j,k)] = sum;
            }
        }
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::generate_parts() {
    size3 part_size = min(this->_part_size, this->_zeta_size_full);
    size3 num_parts = this->_zeta_size_full / part_size;
    const auto n0 = num_parts[0];
    const auto n1 = num_parts[1];
    const auto n2 = num_parts[2];
    const Index<3> index(num_parts);
    this->_parts.reserve(num_parts.product());
    for (uint32_t i=0; i<n0; ++i) {
        for (uint32_t j=0; j<n1; ++j) {
            for (uint32_t k=0; k<n2; ++k) {
                size3 ijk{i,j,k};
                auto begin = ijk*part_size;
                auto offset = min(begin, _phi_size);
                auto end = where(ijk == num_parts-1, this->_zeta_size_full, begin+part_size);
                this->_parts.emplace_back(begin-offset, offset, end, index(i,j,k));
            }
        }
    }
}

template <class T> bool
autoreg::Wave_surface_generator<T>::push_kernels() {
    size3 part_size = min(this->_part_size, this->_zeta_size_full);
    size3 num_parts = this->_zeta_size_full / part_size;
    const auto n0 = num_parts[0];
    const auto n1 = num_parts[1];
    const auto n2 = num_parts[2];
    const Index<3> index(num_parts);
    uint32_t nfinished = 0, nsubmitted = 0;
    for (uint32_t i=0; i<n0; ++i) {
        for (uint32_t j=0; j<n1; ++j) {
            for (uint32_t k=0; k<n2; ++k) {
                auto& part = this->_parts[index(i,j,k)];
                if (part.completed()) { ++nfinished; continue; }
                if (part.submitted()) { ++nsubmitted; continue; }
                int ncompleted = 0, ndependencies = 0;
                auto nl = std::min(i+1, 2u);
                auto nm = std::min(j+1, 2u);
                auto nn = std::min(k+1, 2u);
                for (uint32_t l=0; l<nl; ++l) {
                    for (uint32_t m=0; m<nm; ++m) {
                        for (uint32_t n=0; n<nn; ++n) {
                            if (this->_parts[index(i-l,j-m,k-n)].completed()) {
                                ++ncompleted;
                            }
                            ++ndependencies;
                        }
                    }
                }
                if (ncompleted == ndependencies-1) {
                    //#if defined(AUTOREG_DEBUG)
                    sys::log_message("autoreg", "submit _", part.begin_index());
                    //#endif
                    part.state(Part::State::Submitted);
                    ++nsubmitted;
                    auto k = sbn::make_pointer<Part_generator<T>>(
                        part, _phi_size, this->_phi,
                        this->_zeta[part.slice_in(this->_zeta_size_full)],
                        this->_white_noise_variance, this->_seed);
                    #if defined(AUTOREG_MPI)
                    if (mpi::nranks != 1) {
                        if (this->_subordinate == 0) { ++this->_subordinate; }
                        /*if (this->_subordinate == 0) {
                            k->act();
                            react(std::move(k));
                            ++nfinished;
                        } else*/ {
                            //this->_buffer.clear();
                            auto old_position = this->_output_buffer.position();
                            auto old_size = this->_output_buffer.size();
                            k->write(this->_output_buffer);
                            if (this->_output_buffer.size() != old_size) {
                                sys::log_message("autoreg", "old-size _ new-size _", old_size, this->_output_buffer.size());
                                throw std::runtime_error("buffer overflow");
                            }
                            auto req = mpi::async_send(
                                this->_output_buffer.data()+old_position,
                                int(this->_output_buffer.position() - old_position),
                                this->_subordinate, tag_kernel);
                            this->_requests.emplace_back(std::move(req),
                                                         sbn::kernel_buffer());
                        }
                        if (++this->_subordinate == mpi::nranks) {
                            this->_subordinate = 0;
                        }
                    } else {
                        k->act();
                        react(std::move(k));
                        ++nfinished;
                    }
                    #else
                    sbn::upstream<sbn::Remote>(this, std::move(k));
                    #endif
                }
            }
        }
    }
    //#if defined(AUTOREG_DEBUG)
    sys::log_message("autoreg", "completed _ of _, submitted _", nfinished,
                     this->_parts.size(), nsubmitted);
    //#endif
    if (nfinished == this->_parts.size()) {
        #if defined(AUTOREG_MPI)
        for (int i=1; i<mpi::nranks; ++i) {
            mpi::send<int>(nullptr, 0, i, tag_terminate);
        }
        #endif
        trim_surface(this->_zeta);
        this->_time_points[1] = clock_type::now();
        verify();
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
        return true;
    } else {
        #if defined(AUTOREG_MPI)
        auto status = mpi::probe();
        size_t nbytes = status.num_elements<char>();
        while (this->_buffer.size() < nbytes) {
            this->_buffer.grow();
        }
        this->_buffer.position(0);
        this->_buffer.limit(nbytes);
        mpi::receive(this->_buffer.data(), nbytes, status.source(), status.tag());
        auto k = sbn::make_pointer<Part_generator<T>>();
        k->phase(sbn::kernel::phases::downstream);
        k->read(this->_buffer);
        react(std::move(k));
        remove_completed_requests();
        #endif
        return false;
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::trim_surface(std::valarray<T>& zeta) const {
    const size3 offset = this->_zeta_size_full-this->_zeta_size;
    const std::gslice slice{
        offset.product(),
        {offset[0], offset[1], offset[2]},
        {this->_zeta_size_full[1]*this->_zeta_size_full[2], this->_zeta_size_full[2], 1}
    };
    std::valarray<T> tmp = zeta[slice];
    zeta = std::move(tmp);
}

template <class T> void
autoreg::Wave_surface_generator<T>::verify() {
    this->_time_points[2] = clock_type::now();
    std::valarray<T> reference_zeta(this->_zeta_size_full.product());
    if (this->_verify) {
        generate_white_noise(reference_zeta);
        generate_surface(reference_zeta);
        trim_surface(reference_zeta);
    }
    this->_time_points[3] = clock_type::now();
    {
        std::ofstream log("time.log");
        log << this->_zeta_size[0] << ' ';
        log << this->_zeta_size[1] << ' ';
        log << this->_zeta_size[2] << ' ';
        const auto& t = this->_time_points;
        using namespace std::chrono;
        const auto dt_parallel = duration_cast<microseconds>(t[1]-t[0]).count();
        const auto dt_sequential = duration_cast<microseconds>(t[3]-t[2]).count();
        log << dt_parallel << ' ';
        log << dt_sequential << '\n';
    }
    sys::log_message("autoreg", "mean = _", mean(this->_zeta));
    sys::log_message("autoreg", "variance = _", variance(this->_zeta));
    if (this->_verify) {
        auto residual = abs(reference_zeta - this->_zeta).max();
        sys::log_message("autoreg", "reference mean = _", mean(reference_zeta));
        sys::log_message("autoreg", "reference variance = _", variance(reference_zeta));
        sys::log_message("autoreg", "residual _", residual);
        if (residual != 0) {
            throw std::runtime_error("non-nought residual");
        }
    } else {
        sys::log_message("autoreg", "no verification");
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::write_zeta(const std::valarray<T>& zeta, int time_slice) const {
    std::ofstream out("zeta");
    Vector<int,3> size = this->_zeta_size_full;
    Index<3> zeta_index(size);
    for (int i=time_slice; i<time_slice+1; ++i) {
        for (int j=0; j<size[1]; ++j) {
            for (int k=0; k<size[2]; ++k) {
                out << zeta[zeta_index(i,j,k)] << ' ';
            }
            out << '\n';
        }
    }
}

template <class T> void
autoreg::Part_generator<T>::act() {
    if (const char* hostname = std::getenv("SBN_TEST_SUBORDINATE_FAILURE")) {
        if (sys::this_process::hostname() == hostname) {
            sys::log_message("autoreg", "simulate subordinate failure _!", hostname);
            send(sys::signal::kill, sys::this_process::parent_id());
            send(sys::signal::kill, sys::this_process::id());
        }
    }
    auto t0 = clock_type::now();
    //sys::log_message("autoreg", "part start _ _", this->_part.begin_index(),
    //                 this->_part.end_index());
    const auto& offset = this->_part.offset();
    const Vector<int,3> size = this->_part.end_index() - this->_part.begin_index();
    auto& zeta = this->_zeta;
    auto& phi = this->_phi;
    Index<3> zeta_index(size);
    Index<3> phi_index(this->_phi_size);
    const auto variance = this->_white_noise_variance;
    if (variance < T{}) {
        throw std::runtime_error("white noise variance is less than zero");
    }
    if (std::isnan(variance)) { throw std::runtime_error("white noise variance is NaN"); }
    std::mt19937 generator;
    generator.seed(this->_seed);
    std::normal_distribution<T> normal(T{}, std::sqrt(variance));
    for (int i=offset[0]; i<size[0]; ++i) {
        for (int j=offset[1]; j<size[1]; ++j) {
            for (int k=offset[2]; k<size[2]; ++k) {
                zeta[zeta_index(i,j,k)] = normal(generator);
            }
        }
    }
    for (int i=offset[0]; i<size[0]; ++i) {
        for (int j=offset[1]; j<size[1]; ++j) {
            for (int k=offset[2]; k<size[2]; ++k) {
                const Vector<int,3> lag = min(size3{i+1,j+1,k+1}, this->_phi_size);
                T sum{};
                for (int l=0; l<lag[0]; ++l) {
                    for (int m=0; m<lag[1]; ++m) {
                        for (int n=0; n<lag[2]; ++n) {
                            sum += phi[phi_index(l,m,n)] * zeta[zeta_index(i-l,j-m,k-n)];
                        }
                    }
                }
                zeta[zeta_index(i,j,k)] = sum;
            }
        }
    }
    auto t1 = clock_type::now();
    using namespace std::chrono;
    const auto dt = duration_cast<microseconds>(t1-t0).count();
    const auto ts = duration_cast<microseconds>(t1.time_since_epoch()).count();
    sys::log_message("autoreg", "xxx _ _ _ _",
                     ts,
                     #if defined(AUTOREG_MPI)
                     mpi::name,
                     #else
                     "",
                     #endif
                     dt, this->_part.begin_index());
    #if defined(AUTOREG_MPI)
    phase(sbn::kernel::phases::downstream);
    #else
    sbn::commit<sbn::Remote>(std::move(this_ptr()));
    #endif
}

template <class T> void
autoreg::Part_generator<T>::write(sbn::kernel_buffer& out) const {
    #if !defined(AUTOREG_MPI)
    sbn::kernel::write(out);
    #endif
    auto p0 = out.position();
    out << this->_part << this->_phi_size << this->_phi;
    out << this->_white_noise_variance << this->_seed;
    const Vector<int,3> size = this->_part.end_index() - this->_part.begin_index();
    const Vector<int,3> offset = this->_part.offset();
    const auto& zeta = this->_zeta;
    Index<3> zeta_index(size);
    if (phase() == sbn::kernel::phases::upstream) {
        for (int i=0; i<size[0]; ++i) {
            for (int j=0; j<size[1]; ++j) {
                for (int k=0; k<size[2]; ++k) {
                    if (i >= offset[0] && j >= offset[1] && k >= offset[2]) { continue; }
                    out.write(zeta[zeta_index(i,j,k)]);
                }
            }
        }
    } else {
        for (int i=0; i<size[0]; ++i) {
            for (int j=0; j<size[1]; ++j) {
                for (int k=0; k<size[2]; ++k) {
                    if (i < offset[0] || j < offset[1] || k < offset[2]) { continue; }
                    out.write(zeta[zeta_index(i,j,k)]);
                }
            }
        }
    }
    auto p1 = out.position();
    sys::log_message("autoreg", "written _", p1-p0);
}

template <class T> void
autoreg::Part_generator<T>::read(sbn::kernel_buffer& in) {
    #if !defined(AUTOREG_MPI)
    sbn::kernel::read(in);
    #endif
    in >> this->_part >> this->_phi_size >> this->_phi;
    in >> this->_white_noise_variance >> this->_seed;
    const Vector<int,3> size = this->_part.end_index() - this->_part.begin_index();
    const Vector<int,3> offset = this->_part.offset();
    auto& zeta = this->_zeta;
    Index<3> zeta_index(size);
    zeta.resize(size.product());
    if (phase() == sbn::kernel::phases::upstream) {
        for (int i=0; i<size[0]; ++i) {
            for (int j=0; j<size[1]; ++j) {
                for (int k=0; k<size[2]; ++k) {
                    if (i >= offset[0] && j >= offset[1] && k >= offset[2]) { continue; }
                    in.read(zeta[zeta_index(i,j,k)]);
                }
            }
        }
    } else {
        for (int i=0; i<size[0]; ++i) {
            for (int j=0; j<size[1]; ++j) {
                for (int k=0; k<size[2]; ++k) {
                    if (i < offset[0] || j < offset[1] || k < offset[2]) { continue; }
                    in.read(zeta[zeta_index(i,j,k)]);
                }
            }
        }
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::act() {
    if (const char* hostname = std::getenv("SBN_TEST_SUPERIOR_COPY_FAILURE")) {
        if (sys::this_process::hostname() == hostname) {
            sys::log_message("autoreg", "simulate superior copy failure _!", hostname);
            send(sys::signal::kill, sys::this_process::parent_id());
            send(sys::signal::kill, sys::this_process::id());
        }
    }
    this->_seed = std::chrono::system_clock::now().time_since_epoch().count();
    this->_time_points[0] = clock_type::now();
    #if defined(AUTOREG_MPI)
    mpi::broadcast(&this->_seed, 1);
    mpi::broadcast(&this->_white_noise_variance, 1);
    mpi::broadcast(this->_phi_size.data(), this->_phi_size.size());
    mpi::broadcast(this->_zeta_size_full.data(), this->_zeta_size_full.size());
    mpi::broadcast(this->_zeta_size.data(), this->_zeta_size.size());
    AUTOREG_MPI_SUBORDINATE(this->_phi.resize(this->_phi_size.product()););
    mpi::broadcast(&this->_phi[0], this->_phi.size());
    #endif
    AUTOREG_MPI_SUPERIOR(
        this->_zeta.resize(this->_zeta_size_full.product());
        //generate_white_noise(this->_zeta);
        generate_parts();
        #if defined(AUTOREG_MPI)
        while (!push_kernels()) {}
        #else
        push_kernels();
        #endif
        );
    #if defined(AUTOREG_MPI)
    AUTOREG_MPI_SUBORDINATE(
        while (true) {
            auto status = mpi::probe();
            if (status.tag() == tag_terminate) { break; }
            size_t nbytes = status.num_elements<char>();
            while (this->_buffer.size() < nbytes) {
                this->_buffer.grow();
            }
            this->_buffer.position(0);
            this->_buffer.limit(nbytes);
            mpi::receive(this->_buffer.data(), nbytes, status.source(), status.tag());
            auto k = sbn::make_pointer<Part_generator<T>>();
            k->read(this->_buffer);
            k->act();
            //this->_buffer.clear();
            remove_completed_requests();
            auto old_position = this->_output_buffer.position();
            auto old_size = this->_output_buffer.size();
            k->write(this->_output_buffer);
            if (this->_output_buffer.size() != old_size) {
                sys::log_message("autoreg", "old-size _ new-size _", old_size, this->_output_buffer.size());
                throw std::runtime_error("buffer overflow");
            }
            auto req = mpi::async_send(this->_output_buffer.data()+old_position,
                                       int(this->_output_buffer.position() - old_position),
                                       0, tag_kernel_downstream);
            this->_requests.emplace_back(std::move(req), sbn::kernel_buffer());
        }
    );
    #endif
}

template <class T> void
autoreg::Wave_surface_generator<T>::react(sbn::kernel_ptr&& child) {
    const auto& type = typeid(*child);
    if (type == typeid(Part_generator<T>)) {
        auto tmp = sbn::pointer_dynamic_cast<Part_generator<T>>(std::move(child));
        auto& part = this->_parts[tmp->part().index()];
        part.state(Part::State::Completed);
        this->_zeta[tmp->part().slice_out(this->_zeta_size_full)] =
            tmp->zeta()[tmp->part().slice_out()];
        using namespace std::chrono;
        auto dt = duration_cast<microseconds>(part.total_time()).count();
        sys::log_message("autoreg", "part _ total time _", part.begin_index(), dt);
    }
    #if !defined(AUTOREG_MPI)
    push_kernels();
    #endif
}

#if defined(AUTOREG_MPI)
template <class T> void
autoreg::Wave_surface_generator<T>::remove_completed_requests() {
    auto begin = std::remove_if(
        this->_requests.begin(), this->_requests.end(),
        [] (Request& r) { return r.request().completed(); });
    this->_requests.erase(begin, this->_requests.end());
    if (this->_requests.empty()) {
        this->_output_buffer.position(0);
        this->_output_buffer.limit(this->_output_buffer.size());
    }
}
#endif

template <class T> void
autoreg::Wave_surface_generator<T>::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_phi;
    out << this->_phi_size;
    out << this->_white_noise_variance;
    out << this->_zeta_size_full;
    out << this->_zeta_size;
    out << this->_part_size;
    out << this->_parts;
    out << this->_zeta;
    out << this->_seed;
    for (const auto& tp : this->_time_points) { out << tp; }
    out << this->_verify;
}

template <class T> void
autoreg::Wave_surface_generator<T>::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_phi;
    in >> this->_phi_size;
    in >> this->_white_noise_variance;
    in >> this->_zeta_size_full;
    in >> this->_zeta_size;
    in >> this->_part_size;
    in >> this->_parts;
    in >> this->_zeta;
    in >> this->_seed;
    for (auto& tp : this->_time_points) { in >> tp; }
    in >> this->_verify;
}

template class autoreg::Wave_surface_generator<AUTOREG_REAL_TYPE>;
