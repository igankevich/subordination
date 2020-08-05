#include <fstream>
#include <sstream>
#include <thread>

#include <unistdx/ipc/signal>

#include <autoreg/acf_generator.hh>
#include <autoreg/autoreg_driver.hh>
#include <autoreg/mpi.hh>
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

    template <class V>
    inline void
    write_log(const char* key, V value) {
        sys::log_message("autoreg", "_: _", key, value);
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
    AUTOREG_MPI_SUPERIOR(
        std::clog << std::left << std::boolalpha;
        write_log("ACF size", acf_size);
        write_log("Phi size", _phi_size);
        write_log("Domain", zsize);
        write_log("Domain 2", zsize2);
        write_log("Delta", zdelta);
        /*
        ACF_generator<T> acf_generator(_alpha, _beta, _gamma, acf_delta, acf_size, acf_model);
        acf_generator.act();
        acf_pure = acf_model;
        Autoreg_coefs<T> coefs(acf_model, acf_size, ar_coefs);
        coefs.act();
        Variance_WN<T> varwn(ar_coefs, acf_model);
        varwn.act();
        T var_wn = varwn.get_sum();
        #if defined(AUTOREG_DEBUG)
        sys::log_message("autoreg", "var(acf) = _", acf_model[0]);
        sys::log_message("autoreg", "var(eps) = _", var_wn);
        #endif
        //Wave_surface_generator<T> generator(ar_coefs, _phi_size, var_wn, zsize2, zsize);
        */
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
    );
    #if defined(AUTOREG_MPI)
    AUTOREG_MPI_SUBORDINATE(
        react(std::move(this_ptr()));
    );
    #endif
}

template<class T> void
autoreg::Autoreg_model<T>::react(sbn::kernel_ptr&& child) {
    AUTOREG_MPI_SUPERIOR(
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
                ar_coefs, _phi_size, var_wn, zsize2, zsize, part_size(), this->_verify,
                this->_buffer_size);
            //#if defined(SUBORDINATION_TEST_SLAVE_FAILURE)
            //sbn::upstream(this, std::move(k));
            //#else
            k->setf(sbn::kernel_flag::carries_parent);
            sbn::upstream<sbn::Remote>(this, std::move(k));
            if (const char* hostname = std::getenv("SBN_TEST_SUPERIOR_FAILURE")) {
                if (sys::this_process::hostname() == hostname) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    sys::log_message("autoreg", "simulate superior failure _!", hostname);
                    send(sys::signal::kill, sys::this_process::parent_id());
                    send(sys::signal::kill, sys::this_process::id());
                }
            }
            //#endif
        }
        if (type == typeid(Wave_surface_generator<T>)) {
            //this->parent(nullptr);
            #if defined(AUTOREG_DEBUG)
            sys::log_message("autoreg", "finished all");
            #endif
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        }
    );
    #if defined(AUTOREG_MPI)
    AUTOREG_MPI_SUBORDINATE(
        T var_wn = 0;
        sbn::upstream<sbn::Remote>(
            this, sbn::make_pointer<Wave_surface_generator<T>>(
                ar_coefs, _phi_size, var_wn, zsize2, zsize, part_size(), this->_verify,
                this->_buffer_size));
    );
    #endif
}


template <class T> void
autoreg::Autoreg_model<T>::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out << zsize;
    out << zdelta;
    out << acf_size;
    out << acf_delta;
    out << _phi_size;
    out << zsize2;
    out << _part_size;
    out << _zeta_padding;
    out << acf_model;
    out << acf_pure;
    out << ar_coefs;
    out << _verify;
    out << _homogeneous;
    out << _alpha;
    out << _beta;
    out << _gamma;
}

template <class T> void
autoreg::Autoreg_model<T>::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    if (in.remaining() == 0) {
        if (auto* a = target_application()) {
            const auto& args = a->arguments();
            if (args.size() >= 2) {
                filename(args[1]);
                sys::log_message("autoreg", "filename _", filename());
            }
        }
    } else {
        in >> zsize;
        in >> zdelta;
        in >> acf_size;
        in >> acf_delta;
        in >> _phi_size;
        in >> zsize2;
        in >> _part_size;
        in >> _zeta_padding;
        in >> acf_model;
        in >> acf_pure;
        in >> ar_coefs;
        in >> _verify;
        in >> _homogeneous;
        in >> _alpha;
        in >> _beta;
        in >> _gamma;
    }
}

template <class T> void
autoreg::Autoreg_model<T>::read_input_file() {
    try {
        if (!filename().empty()) {
            std::ifstream cfg;
            cfg.open(filename());
            if (!cfg.is_open()) {
                std::stringstream tmp;
                tmp << "Failed to open \"" << filename() << "\"";
                throw std::runtime_error(tmp.str());
            }
            cfg >> *this;
            cfg.close();
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
    validate_non_nought_product(_phi_size, "phi_size");
    validate_non_nought_product(part_size(), "part_size");
    for (int i=0; i<3; ++i) {
        if (zsize2[i] < zsize[i]) {
            throw std::runtime_error("size_factor < 1, zsize2 < zsize");
        }
    }
    if (_phi_size[0] > this->_part_size[0] ||
        _phi_size[1] > this->_part_size[1] ||
        _phi_size[2] > this->_part_size[2]) {
        std::stringstream tmp;
        tmp << "phi_size[i] > part_size[i], should be 0 < phi_size[i] < part_size[i]\n";
        tmp << "phi_size = " << _phi_size << '\n';
        tmp << "part_size = " << part_size() << '\n';
        throw std::runtime_error(tmp.str());
    }
}

template <class T> void
autoreg::Autoreg_model<T>::num_coefficients(const size3& rhs) {
    this->_phi_size = rhs;
    this->ar_coefs.resize(num_coefficients().product());
}

template <class T> void
autoreg::Autoreg_model<T>::zeta_padding(const size3& rhs) {
    this->_zeta_padding = rhs;
    this->zsize2 = this->zsize + this->_zeta_padding;
}

template<class T>
std::istream&
autoreg::operator>>(std::istream& in, Autoreg_model<T>& m) {
    std::string name;
    while (!getline(in, name, '=').eof()) {
        if (name.size() > 0 && name[0] == '#') in.ignore(1024*1024, '\n');
        else if (name == "zsize") in >> m.zsize;
        else if (name == "zdelta") in >> m.zdelta;
        else if (name == "acf_size") in >> m.acf_size;
        else if (name == "part_size") in >> m._part_size;
        else if (name == "zeta_padding") in >> m._zeta_padding;
        else if (name == "buffer_size") in >> m._buffer_size;
        else if (name == "verify") in >> m._verify;
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
    m.num_coefficients(m.acf_size);
    m.zeta_padding(m._zeta_padding);
    m.acf_delta = m.zdelta;
    m.acf_model.resize(m.acf_size.product());
    m.acf_pure.resize(m.acf_size.product());
    m.validate_parameters();
    return in;
}


template class autoreg::Autoreg_model<AUTOREG_REAL_TYPE>;

template std::istream&
autoreg::operator>>(std::istream& in, Autoreg_model<AUTOREG_REAL_TYPE>& m);
