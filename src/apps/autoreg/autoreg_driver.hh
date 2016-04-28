#ifndef APPS_AUTOREG_AUTOREG_DRIVER_HH
#define APPS_AUTOREG_AUTOREG_DRIVER_HH

namespace autoreg {

template<class T>
std::valarray<T> extract_surface(const std::valarray<T>& z, const size3& zsize, int k) {
	int m = zsize[1];
	int n = zsize[2];
	std::valarray<T> s(m*n);
	Index<3> idx(zsize);
	for (int i=0; i<m; ++i)
		for (int j=0; j<n; ++j)
			s[i*n + j] = z[idx(k, i, j)];
	return s;
}


template<class T>
class Autoreg_model: public Kernel {
public:

	typedef Uniform_grid grid_type;
	typedef Wave_surface_generator<T, grid_type> generator_type;
	typedef stdx::log<Autoreg_model> this_log;

	typedef std::chrono::nanoseconds::rep Time;
	typedef std::chrono::nanoseconds Nanoseconds;
	typedef typename std::make_signed<Time>::type Skew;

	static Time current_time_nano() {
		using namespace std::chrono;
		typedef std::chrono::system_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
	}

	explicit
	Autoreg_model(bool b = true):
	zsize(768, 24, 24),
	zdelta(1, 1, 1),
	acf_size(10, 10, 10),
	acf_delta(zdelta),
	fsize(acf_size),
	linear(false),
	skewness(0),
	kurtosis(0),
	interval(0),
	zsize2(zsize),
	acf_model(acf_size),
	acf_pure(acf_size),
	ar_coefs(fsize),
	homogeneous(b),
	alpha(0.05),
	beta(0.8),
	gamm(1.0)
	{
		validate_parameters();
	}


	void act() override {

		_time0 = current_time_nano();
		std::clog << std::left << std::boolalpha;
		write_log("ACF size:"   , acf_size);
		write_log("Domain:"     , zsize);
		write_log("Domain 2:"   , zsize2);
		write_log("Delta:"      , zdelta);
		write_log("Linear:"     , linear);
		write_log("Skewness:"   , skewness);
		write_log("Kurtosis:"   , kurtosis);
		write_log("Interval:"   , interval);
		write_log("Size factor:", size_factor());

		compute(call(new ACF_generator<T>(alpha, beta, gamm, acf_delta, acf_size, acf_model)));
//		do_it();
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << zsize;
		out << zdelta;
		out << acf_size;
		out << acf_delta;
		out << fsize;
		out << linear;
		out << skewness;
		out << kurtosis;
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
	read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> zsize;
		in >> zdelta;
		in >> acf_size;
		in >> acf_delta;
		in >> fsize;
		in >> linear;
		in >> skewness;
		in >> kurtosis;
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

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			10000,
			"Autoreg_model",
			[] (sys::packetstream& in) {
				Autoreg_model* k = new Autoreg_model;
				k->read(in);
				return k;
			}
		};
	}

	void react(factory::Kernel* child) override;

	void gather_statistics();

	const Domain<T, 3> domain() const { return Domain<T>(zdelta, zsize); }

	template<class V>
	friend istream& operator>>(istream& in, Autoreg_model<V>& model);

private:

	T   size_factor() const { return T(zsize2[0]) / T(zsize[0]); }
	int part_size()       const { return floor(2 * fsize[0]); }
//	int part_size()       const { return zsize[0]; }

	void read_acf(std::istream& in) {
		in >> acf_size;
		acf_model.resize(acf_size);
		acf_pure.resize(acf_size);
		fsize = acf_size;
		in >> acf_model;
	}

	void validate_parameters() {
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
			tmp << "interval >= part_size, should be 0 < fsize[0] < interval < part_size";
			tmp << "fsize[0]  = " << fsize_t << '\n';
			tmp << "interval  = " << interval << '\n';
			tmp << "part_size = " << part_sz << '\n';
			throw std::runtime_error(tmp.str());
		}
		if (fsize_t > part_sz) {
			std::stringstream tmp;
			tmp << "fsize[0] > part_size, should be 0 < fsize[0] < interval < part_size\n";
			tmp << "fsize[0]  = " << fsize_t << '\n';
			tmp << "interval  = " << interval << '\n';
			tmp << "part_size = " << part_sz << '\n';
			throw std::runtime_error(tmp.str());
		}
		if (fsize_t > interval && interval > 0) {
			std::stringstream tmp;
			tmp << "0 < interval < fsize[0], should be 0 < fsize[0] < interval < part_size";
			tmp << "fsize[0]  = " << fsize_t << '\n';
			tmp << "interval  = " << interval << '\n';
			tmp << "part_size = " << part_sz << '\n';
			throw std::runtime_error(tmp.str());
		}
	}
	template<class V>
	void validate_size(const Vector<V, 3>& sz, const char* var_name) {
		if (sz.reduce(std::multiplies<T>()) == 0) {
			std::stringstream str;
			str << "Invalid " << var_name << ": " << sz;
			throw std::runtime_error(str.str().c_str());
		}
	}

	template<class V>
	inline std::ostream& write_log(const char* key, V value) {
		return std::clog << std::setw(20) << key << value << std::endl;
	}

	template<class CDF>
	void write_wave_distribution(CDF cdf) {
		std::ofstream out("skew_normal");
		int count = 100;
		T x0 = -5;
		T x1 = 5;
		T dx = (x1-x0)/count;
		for (int i=0; i<100; ++i) {
			T x = x0 + i*dx;
			out << x << ' ' << cdf(x) << endl;
		}
	}

	void do_it() {
		std::valarray<T> interp_coefs(INTERPOLATION_POLYNOMIAL_ORDER);
		acf_pure = acf_model;
		if (!linear) {
			Skew_normal<T> cdf(skewness, kurtosis);
			//Skew_normal_2<T> cdf(COEF);
			//Weibull<T> cdf(3, 1);
			T breadth = SIGMA_COUNT*sqrt(var_acf(acf_model));
			T nit_x0 = -breadth;
			T nit_x1 =  breadth;
#ifdef DEBUG
			write_wave_distribution(cdf);
#endif
			interpolation_coefs<T>(nit_x0, nit_x1, INTERPOLATION_NODES, interp_coefs, cdf);
			transform_acf<T>(interp_coefs, MAX_NIT_COEFS, acf_model);
		}
		compute(call(new Autoreg_coefs<T>(acf_model, acf_size, ar_coefs)));
	}

	size3 zsize;
	Vector<T, 3> zdelta;
	size3 acf_size;
	Vector<T, 3> acf_delta;
	size3 fsize;

	bool linear;
	T skewness;
	T kurtosis;

	int interval;
	size3 zsize2;

	std::valarray<T> acf_model;
	std::valarray<T> acf_pure;
	std::valarray<T> ar_coefs;

	bool homogeneous;

	// параметры автоковариационной функции
	T alpha, beta, gamm;

	Time _time0;
	Time _time1;

	static const std::size_t INTERPOLATION_POLYNOMIAL_ORDER = 12;      ///< порядок интерполяционного полинома
	static const std::size_t INTERPOLATION_NODES            = 100;     ///< кол-во узлов интерполяции
	static const std::size_t MAX_NIT_COEFS                  = 10;      ///< кол-во коэф. для НБП

	static constexpr T SIGMA_COUNT                      = 3.0;
	static constexpr T COEF                             = 1.2;     ///< коэффициент асимметрии-эксцесса для асимметричного нормального распределения
	};
}


namespace autoreg {

template<class T>
void Autoreg_model<T>::react(factory::Kernel* child) {
	std::clog << "react: child=" << typeid(*child).name() << std::endl;
	if (typeid(*child) == typeid(ACF_generator<T>)) {
		do_it();
	}
	if (typeid(*child) == typeid(Autoreg_coefs<T>)) {
//		write<T>("1.ar_coefs", ar_coefs);
		{ std::ofstream out("ar_coefs"); out << ar_coefs; }
		compute(call(new Variance_WN<T>(ar_coefs, acf_model)));
	}
	if (typeid(*child) == typeid(Variance_WN<T>)) {
		T var_wn = reinterpret_cast<Variance_WN<T>*>(child)->get_sum();
		std::clog << "var_acf=" << var_acf(acf_model) << std::endl;
		std::clog << "var_wn=" << var_wn << std::endl;
		std::size_t max_num_parts = zsize[0] / part_size();
//		std::size_t modulo = homogeneous ? 1 : 2;
		grid_type grid_2(zsize2[0], max_num_parts);
		grid_type grid(zsize[0], max_num_parts);
		generator_type* kernel = new generator_type(
			ar_coefs, fsize, var_wn, zsize2,
			interval, zsize, zdelta, grid, grid_2
		);
		#if defined(FACTORY_TEST_SLAVE_FAILURE)
		compute(call(kernel));
		#else
		spill(carry_parent(kernel));
		#endif
	}
	if (typeid(*child) == typeid(generator_type)) {
		std::clog << "react: done" << std::endl;
		this->parent(nullptr);
//		const std::valarray<T>& water_surface
//			= reinterpret_cast<Wave_surface_generator<T, grid_type>*>(child)->get_water_surface();
		_time1 = current_time_nano();
		{
			std::ofstream timerun_log("time.log");
			timerun_log << float(_time1 - _time0)/1000/1000/1000 << std::endl;
		}
		return_to_parent();
		compute(this);
//		upstream(local_server(), new Velocity_potential<T>(water_surface, zsize, zdelta));
//		if (!linear) {
//			transform_water_surface<T>(interp_coefs, zsize, water_surface, cdf, nit_x0, nit_x1);
//		}
//		if (!homogeneous) {
//			write<T>("ar_coefs", acf_size, ar_coefs);
//			write_all<T>("acf_pure", acf_size, acf_pure);
//			write_all<T>("acf_model", acf_size, acf_model);
//			write_all<T>("water_surface", Domain<T>(zdelta, zsize), water_surface);
			//int l = zsize[0];
			//for (int k=0; k<l; ++k) {
			//	valarray<T> s = extract_surface(water_surface, zsize, k);
			//	size2 ssize(zsize[1], zsize[2]);
			//	int m = 2*ssize[0];
			//	int n = 2*ssize[1];
			//	Domain<T> domain(Vector<T, 3>(T(1), T(1), T(1)), size3(ssize[0], ssize[1], std::size_t(1)));
			//	std::stringstream filename;
			//	filename << "wavy" << k;
			//	ofstream out(filename.str());
			//	for (int i=0; i<m; ++i) {
			//		for (int j=0; j<n; ++j) {
			//			out << lagrange<4, T>(i*T(0.5), j*T(0.5), 0, &s[0], domain) << ' ';
			//		}
			//		out << '\n';
			//	}
			//}
//		}
	}
	if (typeid(*child) == typeid(Velocity_potential<T>)) {
		const Valarray3D<Vector<T, 3>>& phi = reinterpret_cast<Velocity_potential<T>*>(child)->potential();
		const std::valarray<T>& zeta = reinterpret_cast<Velocity_potential<T>*>(child)->zeta();
		std::ofstream out("phi", std::ios::binary);
//		out << Domain<T, 3>(zdelta, zsize) << '\n';
		int nt = zsize[0];
		int nx = zsize[1];
		int ny = zsize[2];
		Index<3> idx(zsize);
		for (int i=0; i<nt; ++i) {
			for (int j=0; j<nx; ++j) {
				for (int k=0; k<ny; ++k) {
					T x = j * zdelta[1];
					T y = k * zdelta[2];
					T z = zeta[idx(i, j, k)];
					T vec[6] = {
						x, y, z,
						phi[i][j][k][0],
						phi[i][j][k][1],
						phi[i][j][k][2]
					};
//					std::clog << vec[0] << ',';
//					std::clog << vec[1] << ',';
//					std::clog << vec[2] << ',';
//					std::clog << vec[3] << ',';
//					std::clog << vec[4] << ',';
//					std::clog << vec[5] << '\n';
					out.write((char*)vec, sizeof(vec));
				}
			}
		}

		commit(local_server());
	}

}


namespace {

enum Input_type {INPUT_TYPE_DEFAULT_ACF, INPUT_TYPE_ACF, INPUT_TYPE_SPECTRUM, INPUT_TYPE_SPECTRUM_FREQUENCY_DIRECTIONAL};

}

template<class T>
istream& operator>>(istream& in, Autoreg_model<T>& m)
{
	std::string name;
	int interval = 0;
	T size_factor = 1.2;
	std::string input_filename;
	Input_type inputType = INPUT_TYPE_ACF;
	int inputTypeCounter = 0;
	while (!getline(in, name, '=').eof()) {
		     if (name.size() > 0 && name[0] == '#') in.ignore(1024*1024, '\n');
		else if (name == "linear"      ) in >> m.linear;
		else if (name == "skewness"    ) in >> m.skewness;
		else if (name == "kurtosis"    ) in >> m.kurtosis;
		else if (name == "zsize"       ) in >> m.zsize;
		else if (name == "zdelta"      ) in >> m.zdelta;
		else if (name == "acf_size"    ) in >> m.acf_size;
		else if (name == "size_factor" ) in >> size_factor;
		else if (name == "interval"    ) in >> interval;
		else if (name == "alpha"       ) in >> m.alpha;
		else if (name == "beta"        ) in >> m.beta;
		else if (name == "gamma"       ) in >> m.gamm;
		else if (name == "acf"         ) { in >> input_filename; inputType = INPUT_TYPE_ACF; inputTypeCounter++; }
//		else if (name == "spectrum"    ) { in >> input_filename; inputType = INPUT_TYPE_SPECTRUM; inputTypeCounter++; }
//		else if (name == "spectrum_fd" ) { in >> input_filename; inputType = INPUT_TYPE_SPECTRUM_FREQUENCY_DIRECTIONAL; inputTypeCounter++; }
		else {
			in.ignore(1024*1024, '\n');
			std::stringstream str;
			str << "Unknown parameter: " << name << '.';
			throw std::runtime_error(str.str().c_str());
		}
		in >> std::ws;
	}

	if (inputTypeCounter > 1) {
		std::stringstream str;
		str << "Multiple ACF or spectrum are specified.";
		throw std::runtime_error(str.str().c_str());
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

	// read ACF
	if (inputType == INPUT_TYPE_ACF) {
		if (!input_filename.empty()) {
			std::ifstream file(input_filename.c_str());
			m.read_acf(file);
		} else {
//			m.read_acf(std::cin);
		}
//		std::cerr << "Neither ACF nor spectrum are specified. Generating default ACF.\n";
//		approx_acf<T>(m.alpha, m.beta, m.gamm, m.acf_delta, m.acf_size, m.acf_model);
	}

//	// generate ACF
//	if (inputType == INPUT_TYPE_DEFAULT_ACF) {
//		std::cerr << "Neither ACF nor spectrum are specified. Generating default ACF.\n";
//		approx_acf<T>(m.alpha, m.beta, m.gamm, m.acf_delta, m.acf_size, m.acf_model);
//	}
//
//	// read spectrum and convert it to ACF using FFT
//	if (inputType == INPUT_TYPE_SPECTRUM || inputType == INPUT_TYPE_SPECTRUM_FREQUENCY_DIRECTIONAL) {
//		Domain<T, 2> spectrum_domain;
//		std::ifstream in(input_filename.c_str());
//		in >> spectrum_domain;
//		size2 spectrum_size = spectrum_domain.count();
//		std::valarray<T> spectrum(spectrum_size);
//		in >> spectrum;
//
//		int nx = spectrum_size[0];
//		int ny = spectrum_size[1];
//		T g = 9.8;
//
//		if (inputType == INPUT_TYPE_SPECTRUM_FREQUENCY_DIRECTIONAL) {
//
//			// transform to spatial spectrum using corresponding jacobian
//			for (int i=0; i<nx; ++i) {
//				for (int j=0; j<ny; ++j) {
//					T w = spectrum_domain.point(size2{i, j})[0];
//					T jacobian = T(2) * w*w*w / (g*g);
//					spectrum[i*ny + j] *= jacobian;
//				}
//			}
//
//			// transform min value
//			Vector<T, 2> x0 = spectrum_domain.min();
//			T w = x0[0];
//			T theta = x0[1];
//			spectrum_domain.min() = w*w * Vector<T, 2>(cos(theta), sin(theta)) / g;
//
//			// transform max value
//			Vector<T, 2> x1 = spectrum_domain.max();
//			w = x1[0];
//			theta = x1[1];
//			spectrum_domain.max() = w*w * Vector<T, 2>(cos(theta), sin(theta)) / g;
//		}
//
//		int mt = m.acf_size[0];
//		int mx = m.acf_size[1];
//		int my = m.acf_size[2];
//		Index<3> idx(m.acf_size);
//		Vector<T, 2> spectrum_delta = spectrum_domain.delta();
//		T du = spectrum_delta[1];
//		T dv = spectrum_delta[2];
//		for (int c=0; c<mt; ++c) {
//			for (int p=0; p<mx; ++p) {
//				for (int q=0; q<my; ++q) {
//					T t = c * m.acf_delta[0];
//					T x = p * m.acf_delta[1];
//					T y = q * m.acf_delta[2];
//					T sum = 0;
//					for (int i=0; i<nx; ++i) {
//						for (int j=0; j<ny; ++j) {
//							Vector<T, 2> pt = spectrum_domain.point(size2{i, j});
//							T u = pt[0];
//							T v = pt[1];
//							T omega = sqrt(g*(u*u + v*v));
//							sum += spectrum[i*ny + j] * cos(x*u + y*v - omega*t);
//						}
//					}
//					m.acf_model[idx(c, p, q)] = sum*du*dv;
//				}
//			}
//		}
//		{ std::ofstream out("acf_check"); out << Domain<T, 3>(m.acf_size, m.acf_delta) << m.acf_model; }
//		exit(0);
//	}

	m.validate_parameters();

	return in;
}

}

#endif // APPS_AUTOREG_AUTOREG_DRIVER_HH
