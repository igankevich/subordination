#ifndef EXAMPLES_AUTOREG_AUTOREG_DRIVER_HH
#define EXAMPLES_AUTOREG_AUTOREG_DRIVER_HH

namespace autoreg {

    template<class T>
    class Autoreg_model: public sbn::kernel {
    public:

        typedef Uniform_grid grid_type;
        typedef Wave_surface_generator<T, grid_type> generator_type;

        typedef std::chrono::nanoseconds::rep Time;
        typedef std::chrono::nanoseconds Nanoseconds;

        static Time
        current_time_nano() {
            using namespace std::chrono;
            typedef std::chrono::system_clock clock_type;
            return duration_cast<nanoseconds>(
                clock_type::now().time_since_epoch()
            ).count();
        }

        explicit
        Autoreg_model(bool b = true):
        zsize(200, 32, 32),
        zdelta(1, 1, 1),
        acf_size(10, 10, 10),
        acf_delta(zdelta),
        fsize(acf_size),
        interval(0),
        zsize2(zsize*1.4),
        acf_model(acf_size),
        acf_pure(acf_size),
        ar_coefs(fsize),
        homogeneous(b),
        alpha(0.05),
        beta(0.8),
        gamm(1.0) {
            validate_parameters();
        }

        void
        act() override {

            _time0 = current_time_nano();
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
                    alpha,
                    beta,
                    gamm,
                    acf_delta,
                    acf_size,
                    acf_model
                )
            );
//		do_it();
        }

        void
        write(sbn::kernel_buffer& out) const override {
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
            out << homogeneous;
            out << alpha;
            out << beta;
            out << gamm;
            out << _time0 << _time1;
        }

        void
        read(sbn::kernel_buffer& in) override {
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
            in >> homogeneous;
            in >> alpha;
            in >> beta;
            in >> gamm;
            in >> _time0 >> _time1;
        }

        void
        react(sbn::kernel_ptr&& child) override;

        inline const Domain<T, 3>
        domain() const noexcept {
            return Domain<T>(zdelta, zsize);
        }

        template<class V>
        friend std::istream&
        operator>>(std::istream& in, Autoreg_model<V>& model);

    private:

        inline T
        size_factor() const noexcept {
            return T(zsize2[0]) / T(zsize[0]);
        }

        inline int
        part_size() const noexcept {
            return floor(2 * fsize[0]);
        }

        void
        validate_parameters() {
            validate_size<std::size_t>(zsize, "zsize");
            validate_size<T>(zdelta, "zdelta");
            validate_size<std::size_t>(acf_size, "acf_size");
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

        template<class V>
        void
        validate_size(const Vector<V, 3>& sz, const char* var_name) {
            if (sz.reduce(std::multiplies<T>()) == 0) {
                std::stringstream str;
                str << "Invalid " << var_name << ": " << sz;
                throw std::runtime_error(str.str().c_str());
            }
        }

        template<class V>
        inline std::ostream&
        write_log(const char* key, V value) {
            return std::clog << std::setw(20) << key << value << std::endl;
        }

        void
        do_it() {
            acf_pure = acf_model;
            sbn::upstream(
                this,
                sbn::make_pointer<Autoreg_coefs<T>>(acf_model, acf_size, ar_coefs)
            );
        }

        size3 zsize;
        Vector<T, 3> zdelta;
        size3 acf_size;
        Vector<T, 3> acf_delta;
        size3 fsize;

        int interval;
        size3 zsize2;

        std::valarray<T> acf_model;
        std::valarray<T> acf_pure;
        std::valarray<T> ar_coefs;

        bool homogeneous;

        /// ACF parameters.
        T alpha, beta, gamm;

        Time _time0;
        Time _time1;
    };
}


namespace autoreg {

    template<class T>
    void
    Autoreg_model<T>::react(sbn::kernel_ptr&& child) {
    #if defined(SBN_DEBUG)
        sys::log_message("autoreg", "finished _", typeid(*child).name());
    #endif
        const auto& tid = typeid(*child);
        if (tid == typeid(ACF_generator<T>)) {
            do_it();
        }
        if (tid == typeid(Autoreg_coefs<T>)) {
//		write<T>("1.ar_coefs", ar_coefs);
            { std::ofstream out("ar_coefs"); out << ar_coefs; }
            sbn::upstream(this, sbn::make_pointer<Variance_WN<T>>(ar_coefs, acf_model));
        }
        if (tid == typeid(Variance_WN<T>)) {
            auto tmp = sbn::pointer_dynamic_cast<Variance_WN<T>>(std::move(child));
            T var_wn = tmp->get_sum();
        #if defined(SBN_DEBUG)
            sys::log_message("autoreg", "var(acf) = _", var_acf(acf_model));
            sys::log_message("autoreg", "var(eps) = _", var_wn);
        #endif
            std::size_t max_num_parts = zsize[0] / part_size();
//		std::size_t modulo = homogeneous ? 1 : 2;
            grid_type grid_2(zsize2[0], max_num_parts);
            grid_type grid(zsize[0], max_num_parts);
            auto k =
                sbn::make_pointer<generator_type>(
                    ar_coefs,
                    fsize,
                    var_wn,
                    zsize2,
                    interval,
                    zsize,
                    zdelta,
                    grid,
                    grid_2
                );
        #if defined(SUBORDINATION_TEST_SLAVE_FAILURE)
            sbn::upstream(this, std::move(k));
        #else
            //k->setf(sbn::kernel_flag::carries_parent);
            sbn::upstream<sbn::Remote>(this, std::move(k));
        #endif
        }
        if (tid == typeid(generator_type)) {
            //this->parent(nullptr);
            _time1 = current_time_nano();
            {
                std::ofstream timerun_log("time.log");
                timerun_log << float(_time1 - _time0)/1000/1000/1000 << std::endl;
            }
            sbn::commit(std::move(this_ptr()));
        }

    }

    template<class T>
    std::istream&
    operator>>(std::istream& in, Autoreg_model<T>& m) {
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
            else if (name == "alpha") in >> m.alpha;
            else if (name == "beta") in >> m.beta;
            else if (name == "gamma") in >> m.gamm;
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
        m.acf_model.resize(m.acf_size);
        m.acf_pure.resize(m.acf_size);
        m.ar_coefs.resize(m.fsize);
        m.validate_parameters();
        return in;
    }

}

#endif // vim:filetype=cpp
