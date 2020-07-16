#include <fstream>
#include <sstream>

#include <autoreg/acf_generator.hh>
#include <autoreg/autoreg_driver.hh>
#include <autoreg/variance_wn.hh>

namespace  {

    template <class T> inline void
    validate_non_nought_product(const autoreg::Vector<T,3>& v, const char* var_name) {
        if (v.product() == T{}) {
            std::stringstream str;
            str << "Invalid " << var_name << ": " << v;
            throw std::runtime_error(str.str().c_str());
        }
    }

    template<class V>
    inline std::ostream&
    write_log(const char* key, V value) {
        return std::clog << std::setw(20) << key << value << std::endl;
    }

    inline uint64_t
    current_time_nano() {
        using namespace std::chrono;
        using C = std::chrono::system_clock;
        return duration_cast<nanoseconds>(C::now().time_since_epoch()).count();
    }

}

template <class T> void
autoreg::Autoreg_model<T>::act() {
    read_input_file();
    std::clog << std::left << std::boolalpha;
    write_log("ACF size:", acf_size);
    write_log("Domain:", zsize);
    write_log("Domain 2:", zsize2);
    write_log("Delta:", zdelta);
    write_log("Interval:", interval);
    write_log("Size factor:", size_factor());
    sbn::upstream(
        this,
        sbn::make_pointer<ACF_generator<T>>(
            _alpha,
            _beta,
            _gamma,
            acf_delta,
            acf_size,
            acf_model
        )
    );
}

template<class T> void
autoreg::Autoreg_model<T>::react(sbn::kernel_ptr&& child) {
    #if defined(AUTOREG_DEBUG)
    sys::log_message("autoreg", "finished _", typeid(*child).name());
    #endif
    const auto& type = typeid(*child);
    if (type == typeid(ACF_generator<T>)) {
        acf_pure = acf_model;
        sbn::upstream(
            this,
            sbn::make_pointer<Autoreg_coefs<T>>(acf_model, acf_size, ar_coefs)
        );
    }
    if (type == typeid(Autoreg_coefs<T>)) {
//		write<T>("1.ar_coefs", ar_coefs);
        //{ std::ofstream out("ar_coefs"); out << ar_coefs; }
        sbn::upstream(this, sbn::make_pointer<Variance_WN<T>>(ar_coefs, acf_model));
    }
    if (type == typeid(Variance_WN<T>)) {
        auto tmp = sbn::pointer_dynamic_cast<Variance_WN<T>>(std::move(child));
        T var_wn = tmp->get_sum();
        #if defined(AUTOREG_DEBUG)
        sys::log_message("autoreg", "var(acf) = _", acf_model[0]);
        sys::log_message("autoreg", "var(eps) = _", var_wn);
        #endif
        auto k = sbn::make_pointer<Wave_surface_generator<T>>(
            ar_coefs, fsize, var_wn, zsize2, zsize);
        //#if defined(SUBORDINATION_TEST_SLAVE_FAILURE)
        //sbn::upstream(this, std::move(k));
        //#else
        //k->setf(sbn::kernel_flag::carries_parent);
        sbn::upstream<sbn::Remote>(this, std::move(k));
        //#endif
    }
    if (type == typeid(Wave_surface_generator<T>)) {
        //this->parent(nullptr);
        #if defined(AUTOREG_DEBUG)
        sys::log_message("autoreg", "finished all");
        #endif
        sbn::commit(std::move(this_ptr()));
    }
}


template <class T> void
autoreg::Autoreg_model<T>::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out << zsize;
    out << zdelta;
    out << acf_size;
    out << acf_delta;
    out << fsize;
    out << interval;
    out << zsize2;
    out << acf_model;
    out << acf_pure;
    out << ar_coefs;
    out << _homogeneous;
    out << _alpha;
    out << _beta;
    out << _gamma;
}

template <class T> void
autoreg::Autoreg_model<T>::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    in >> zsize;
    in >> zdelta;
    in >> acf_size;
    in >> acf_delta;
    in >> fsize;
    in >> interval;
    in >> zsize2;
    in >> acf_model;
    in >> acf_pure;
    in >> ar_coefs;
    in >> _homogeneous;
    in >> _alpha;
    in >> _beta;
    in >> _gamma;
}

template <class T> void
autoreg::Autoreg_model<T>::read_input_file() {
    try {
        std::ifstream cfg(this->_filename);
        if (cfg.is_open()) {
            cfg >> *this;
        } else {
            #if defined(AUTOREG_DEBUG)
            sys::log_message("autoreg", "using default parameters");
            #endif
        }
    } catch (const std::exception& e) {
        std::cerr << e.what();
        std::exit(1);
    }
}

template <class T> void
autoreg::Autoreg_model<T>::validate_parameters() {
    validate_non_nought_product(zsize, "zsize");
    validate_non_nought_product(zdelta, "zdelta");
    validate_non_nought_product(acf_size, "acf_size");
    for (int i=0; i<3; ++i) {
        if (zsize2[i] < zsize[i]) {
            throw std::runtime_error("size_factor < 1, zsize2 < zsize");
        }
    }
    int part_sz = part_size();
    int fsize_t = fsize[0];
    if (interval >= part_sz) {
        std::stringstream tmp;
        tmp <<
            "interval >= part_size, should be 0 < fsize[0] < interval < part_size";
        tmp << "fsize[0]  = " << fsize_t << '\n';
        tmp << "interval  = " << interval << '\n';
        tmp << "part_size = " << part_sz << '\n';
        throw std::runtime_error(tmp.str());
    }
    if (fsize_t > part_sz) {
        std::stringstream tmp;
        tmp <<
            "fsize[0] > part_size, should be 0 < fsize[0] < interval < part_size\n";
        tmp << "fsize[0]  = " << fsize_t << '\n';
        tmp << "interval  = " << interval << '\n';
        tmp << "part_size = " << part_sz << '\n';
        throw std::runtime_error(tmp.str());
    }
    if (fsize_t > interval && interval > 0) {
        std::stringstream tmp;
        tmp <<
            "0 < interval < fsize[0], should be 0 < fsize[0] < interval < part_size";
        tmp << "fsize[0]  = " << fsize_t << '\n';
        tmp << "interval  = " << interval << '\n';
        tmp << "part_size = " << part_sz << '\n';
        throw std::runtime_error(tmp.str());
    }
}


template<class T>
std::istream&
autoreg::operator>>(std::istream& in, Autoreg_model<T>& m) {
    std::string name;
    int interval = 0;
    T size_factor = 1.2;
    while (!getline(in, name, '=').eof()) {
        if (name.size() > 0 && name[0] == '#') in.ignore(1024*1024, '\n');
        else if (name == "zsize") in >> m.zsize;
        else if (name == "zdelta") in >> m.zdelta;
        else if (name == "acf_size") in >> m.acf_size;
        else if (name == "size_factor") in >> size_factor;
        else if (name == "interval") in >> interval;
        else if (name == "alpha") in >> m._alpha;
        else if (name == "beta") in >> m._beta;
        else if (name == "gamma") in >> m._gamma;
        else {
            in.ignore(1024*1024, '\n');
            std::stringstream str;
            str << "Unknown parameter: " << name << '.';
            throw std::runtime_error(str.str().c_str());
        }
        in >> std::ws;
    }
    if (size_factor < T(1)) {
        std::stringstream str;
        str << "Invalid size factor: " << size_factor;
        throw std::runtime_error(str.str().c_str());
    }
    m.zsize2 = m.zsize*size_factor;
    m.interval = interval;
    m.acf_delta = m.zdelta;
    m.fsize = m.acf_size;
    m.acf_model.resize(m.acf_size.product());
    m.acf_pure.resize(m.acf_size.product());
    m.ar_coefs.resize(m.fsize.product());
    m.validate_parameters();
    return in;
}


template class autoreg::Autoreg_model<AUTOREG_REAL_TYPE>;

template std::istream&
autoreg::operator>>(std::istream& in, Autoreg_model<AUTOREG_REAL_TYPE>& m);
