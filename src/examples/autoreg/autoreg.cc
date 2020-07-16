#include <random>

#include <autoreg/autoreg.hh>

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
    zeta.resize(zsize2.product());
    std::mt19937 generator;
    generator.seed(this->_seed);
    std::normal_distribution<T> normal(T{}, std::sqrt(variance));
    for (auto& z : zeta) { z = normal(generator); }
    for (const auto& z : zeta) {
        if (std::isnan(z)) {
            throw std::runtime_error("white noise generator produced some NaNs");
        }
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::generate_surface(std::valarray<T>& zeta) const {
    auto& phi = this->_phi;
    Vector<int,3> size = zsize2;
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
    size3 part_size = min(_phi_size*2, zsize2);
    size3 num_parts = zsize2 / part_size;
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
                auto end = where(ijk == num_parts-1, zsize2, begin+part_size);
                this->_parts.emplace_back(begin-offset, offset, end, index(i,j,k));
            }
        }
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::push_kernels() {
    size3 part_size = min(_phi_size*2, zsize2);
    size3 num_parts = zsize2 / part_size;
    const auto n0 = num_parts[0];
    const auto n1 = num_parts[1];
    const auto n2 = num_parts[2];
    const Index<3> index(num_parts);
    uint32_t nfinished = 0;
    for (uint32_t i=0; i<n0; ++i) {
        for (uint32_t j=0; j<n1; ++j) {
            for (uint32_t k=0; k<n2; ++k) {
                auto& part = this->_parts[index(i,j,k)];
                if (part.completed()) { ++nfinished; continue; }
                if (part.submitted()) { continue; }
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
                    #if defined(AUTOREG_DEBUG)
                    sys::log_message("autoreg", "submit _", part.begin_index());
                    #endif
                    part.state(Part::State::Submitted);
                    sbn::upstream<sbn::Remote>(
                        this, sbn::make_pointer<Part_generator<T>>(
                            part, _phi_size, this->_phi, this->_zeta[part.slice_in(zsize2)]));
                }
            }
        }
    }
    #if defined(AUTOREG_DEBUG)
    sys::log_message("autoreg", "completed _ of _", nfinished, this->_parts.size());
    #endif
    if (nfinished == this->_parts.size()) {
        trim_surface(this->_zeta);
        this->_time_points[1] = clock_type::now();
        verify();
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::trim_surface(std::valarray<T>& zeta) const {
    const size3 offset = zsize2-zsize;
    const std::gslice slice{
        offset.product(),
        {offset[0], offset[1], offset[2]},
        {zsize2[1]*zsize2[2], zsize2[2], 1}
    };
    std::valarray<T> tmp = zeta[slice];
    zeta = std::move(tmp);
}

template <class T> void
autoreg::Wave_surface_generator<T>::verify() {
    this->_time_points[2] = clock_type::now();
    std::valarray<T> reference_zeta(zsize2.product());
    generate_white_noise(reference_zeta);
    generate_surface(reference_zeta);
    trim_surface(reference_zeta);
    this->_time_points[3] = clock_type::now();
    {
        std::ofstream log("time.log");
        log << zsize[0] << ' ' << zsize[1] << ' ' << zsize[2] << ' ';
        const auto& t = this->_time_points;
        using namespace std::chrono;
        log << duration_cast<microseconds>(t[1]-t[0]).count() << ' ';
        log << duration_cast<microseconds>(t[3]-t[2]).count() << '\n';
    }
    auto residual = abs(reference_zeta - this->_zeta).max();
    sys::log_message("autoreg", "mean = _", mean(this->_zeta));
    sys::log_message("autoreg", "variance = _", variance(this->_zeta));
    sys::log_message("autoreg", "reference mean = _", mean(reference_zeta));
    sys::log_message("autoreg", "reference variance = _", variance(reference_zeta));
    sys::log_message("autoreg", "residual _", residual);
    if (residual != 0) {
        throw std::runtime_error("non-nought residual");
    }
}

template <class T> void
autoreg::Wave_surface_generator<T>::write_zeta(const std::valarray<T>& zeta, int time_slice) const {
    std::ofstream out("zeta");
    Vector<int,3> size = zsize2;
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
    const auto& offset = this->_part.offset();
    const Vector<int,3> size = this->_part.end_index() - this->_part.begin_index();
    auto& zeta = this->_zeta;
    auto& phi = this->_phi;
    Index<3> zeta_index(size);
    Index<3> phi_index(this->_phi_size);
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
    sbn::commit<sbn::Remote>(std::move(this_ptr()));
}

template <class T> void
autoreg::Part_generator<T>::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_part << this->_phi_size << this->_phi << this->_zeta;
}

template <class T> void
autoreg::Part_generator<T>::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_part >> this->_phi_size >> this->_phi >> this->_zeta;
}

template <class T> void
autoreg::Wave_surface_generator<T>::act() {
    this->_seed = std::chrono::system_clock::now().time_since_epoch().count();
    this->_time_points[0] = clock_type::now();
    generate_white_noise(this->_zeta);
    generate_parts();
    push_kernels();
}

template <class T> void
autoreg::Wave_surface_generator<T>::react(sbn::kernel_ptr&& child) {
    const auto& type = typeid(*child);
    if (type == typeid(Part_generator<T>)) {
        auto tmp = sbn::pointer_dynamic_cast<Part_generator<T>>(std::move(child));
        this->_parts[tmp->part().index()].state(Part::State::Completed);
        this->_zeta[tmp->part().slice_out(zsize2)] = tmp->zeta()[tmp->part().slice_out()];
    }
    push_kernels();
}

template <class T> void
autoreg::Wave_surface_generator<T>::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << _phi;
    out << _phi_size;
    out << _white_noise_variance;
    out << zsize2;
    out << zsize;
    out << this->_parts;
    out << this->_zeta;
    out << this->_seed;
    for (const auto& tp : this->_time_points) { out << tp; }
}

template <class T> void
autoreg::Wave_surface_generator<T>::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> _phi;
    in >> _phi_size;
    in >> _white_noise_variance;
    in >> zsize2;
    in >> zsize;
    in >> this->_parts;
    in >> this->_zeta;
    in >> this->_seed;
    for (auto& tp : this->_time_points) { in >> tp; }
}

template class autoreg::Wave_surface_generator<AUTOREG_REAL_TYPE>;
