#ifndef APPS_AUTOREG_AUTOREG_HH
#define APPS_AUTOREG_AUTOREG_HH

#include <random>
#include <chrono>

#include "discrete_function.hh"
#include "mersenne_twister.hh"
#include "grid.hh"

namespace std {

	template<class T>
	sys::packetstream&
	operator<<(sys::packetstream& out, const std::valarray<T>& rhs) {
		out << uint32_t(rhs.size());
		for (size_t i=0; i<rhs.size(); ++i) {
			out << rhs[i];
		}
		return out;
	}

	template<class T>
	sys::packetstream&
	operator>>(sys::packetstream& in, std::valarray<T>& rhs) {
		uint32_t n = 0;
		in >> n;
		rhs.resize(n);
		for (size_t i=0; i<rhs.size(); ++i) {
			in >> rhs[i];
		}
		return in;
	}

}

namespace stdx {

	template<class T>
	bool
	isnan(T rhs) noexcept {
		return std::isnan(rhs);
	}

}

namespace autoreg {

template<class Container>
typename Container::value_type
product(const Container& rhs) {
	typedef typename Container::value_type T;
	return std::accumulate(
		std::begin(rhs), std::end(rhs), T(1), std::multiplies<T>()
	);
}

template<class T>
void
transpose(std::valarray<T>& rhs, size3 size) {
	Index<3> idx(size);
	size_t m1 = size[0];
	size_t m2 = size[1];
	size_t m3 = size[2];
	for (size_t k=0; k<m1; k++)
		for (size_t i=0; i<m2; i++)
			for (size_t j=0; j<m3; j++)
				rhs[idx(k, i, j)] = rhs[idx(j, i, k)];
}

template<class T>
inline T approx_wave_height(T variance) {
	return sqrt(T(2)*M_PI*(variance));
}

template<class T>
inline T approx_wave_period(T variance) {
	return T(4.8)*sqrt(approx_wave_height(variance));
}


}
namespace {

using namespace autoreg;

template<class T>
struct Variance_WN: public Kernel {

	Variance_WN(const std::valarray<T>& ar_coefs_, const std::valarray<T>& acf_):
		ar_coefs(ar_coefs_), acf(acf_), _sum(0) {}

	void act() override {
		int bs = 64;
		int n = ar_coefs.size();
		factory::upstream(local_server, this, mapreduce([](int) {}, [this](int i){
			_sum += ar_coefs[i]*acf[i];
		}, 0, n, bs));
	}

	void react(factory::Kernel*) override {
		_sum = acf[0] - _sum;
		factory::commit(local_server, this);
	}

	T
	get_sum() const noexcept {
		return _sum;
	}

private:

	const std::valarray<T>& ar_coefs;
	const std::valarray<T>& acf;
	T _sum;

};

template<class T> T var_acf(std::valarray<T>& acf) { return acf[0]; }

template<class T>
struct Yule_walker: public Kernel {

	Yule_walker(const std::valarray<T>& acf_,
				 const size3& acf_size_,
				 std::valarray<T>& a_,
				 std::valarray<T>& b_):
		acf(acf_), acf_size(acf_size_), a(a_), b(b_), count(0) {}

	void act() override {
		int n = acf.size() - 1;
		int block_size = 16*4;
		int m = b.size();
		auto identity = [](int){};
		factory::upstream(local_server, this, mapreduce([this](int i) {
			const int n = acf.size()-1;
			const Index<3> id(acf_size);
			const Index<2> ida(size2(n, n));
			for (int j=0; j<n; j++) {
			    // casting to signed type ptrdiff_t
				int i2 = id(abs(id.t(i+1), id.t(j+1)),
				            abs(id.x(i+1), id.x(j+1)),
				            abs(id.y(i+1), id.y(j+1)) );
				int i1 = i*n + j; //ida(i, j);
//				if (i == 1) {
//					std::cout << "i  = " << i << endl;
//					std::cout << "j  = " << j << endl;
//					std::cout << "i2 = " << i2 << endl;
//				}
				a[i1] = acf[i2];
			}
		}, identity, 0, n, block_size));
		factory::upstream(local_server, this, mapreduce([this](int i) {
			const Index<3> id(acf_size);
			b[i] = acf[id( id.t(i+1), id.x(i+1), id.y(i+1) )];
		}, identity, 0, m, block_size));
	}

	void react(factory::Kernel*) override {
		if (++count == 2) {
			factory::commit(local_server, this);
		}
	}

private:
	inline int abs(int a, int b) {
	    return (a > b) ? a-b : b-a;
	}

	const std::valarray<T>& acf;
	const size3& acf_size;
	std::valarray<T>& a;
	std::valarray<T>& b;
	int count;
};

template<class T>
bool is_stationary(std::valarray<T>& ar_coefs) {
	int n = ar_coefs.size();
	for (int i=0; i<n; ++i)
		if (std::abs(ar_coefs[i]) > T(1))
			return false;
	return true;
}

}

namespace autoreg {

template<class T>
void approx_acf(const T alpha,
                const T beta,
				const T gamm,
				const Vector<T, 3>& delta,
				const size3& acf_size,
				std::valarray<T>& acf)
{
	const Index<3> id(acf_size);
    for (std::size_t t=0; t<acf_size[0]; t++) {
        for (std::size_t x=0; x<acf_size[1]; x++) {
            for (std::size_t y=0; y<acf_size[2]; y++) {
				const T k1 = t*delta[0] + x*delta[1] + y*delta[2];
				acf[id(t, x, y)] = gamm*exp(-alpha*k1)*cos(beta*t*delta[0])*cos(beta*x*delta[1]);//*cos(beta*y*delta[2]);
			}
		}
	}
}

//template<class T>
//struct ACF_sub;

template<class T>
struct ACF_generator: public Kernel {

	typedef autoreg::Discrete_function<T, 2> Spectrum;
	typedef autoreg::Discrete_function<T, 3> ACF;
	typedef int I;

	ACF_generator(const T alpha_,
        const T beta_,
		const T gamm_,
		const Vector<T, 3>& delta_,
		const size3& acf_size_,
		std::valarray<T>& acf_):
		alpha(alpha_), beta(beta_), gamm(gamm_),
		delta(delta_), acf_size(acf_size_), acf(acf_)
	{}

	void act() override {
//		Spectrum spec{
//			{64, 64},
//			{T(-0.3), T(0)},
//			{T( 0.3), T(1.5)}
//		};
//		spec([this, &spec] (T& val, int idx[2]) {
//			const T _omega_max = 1.02;
//			const T _theta_max = 1.5;
//			const I _n = 4;
//			const I _m = 2;
//			auto pt = spec.domain_point(idx);
//			T u = pt[0];
//			T v = pt[1];
//			T disp = std::pow(u*u + v*v, T(0.25));
//			T omega = std::sqrt(G) * disp;
//			T theta = std::atan2(v, u);
//			T jacobian = disp == T(0) ? T(1) : T(0.5) * std::sqrt(G) / std::pow(disp, 3);
////			jacobian = T(2) * std::pow(omega, 3) / G / G;
//			jacobian = 1;
//			jacobian *= 0.4;
//			val = autoreg::frequency_spec(omega, _omega_max, _n)
//				* autoreg::directional_spec(theta, _theta_max, _m)
//				* jacobian;
//		});
//		{
//			std::ofstream out("spec.blob");
//			spec.write(out);
//		}
//
//		auto delta = spec.domain_delta();
//		T du = delta[0];
//		T dv = delta[1];
//
//		const Index<3> id(acf_size);
//		int t1 = acf_size[0];
//		int x1 = acf_size[1];
//		int y1 = acf_size[2];
//		for (int t=0; t<t1; t++) {
//		    for (int x=0; x<x1; x++) {
//		        for (int y=0; y<y1; y++) {
//					std::complex<T> sum = 0;
//					spec([this, &spec, &sum, &t, &x, &y, &du, &dv] (T val2, int idx2[2]) {
//						auto pt = spec.domain_point(idx2);
//						T u = pt[0];
//						T v = pt[1];
//						T w = std::sqrt(G*(u*u + v*v));
//		//				sum += val2 * std::cos(x*u + y*v - w*t);
//						sum += val2 * std::exp(std::complex<T>(T(0), -w*t + x*u + y*v));
//					});
//					acf[id(t, x, y)] = (T(1)/PI) * std::norm(sum)*du*dv;// * std::exp(-alpha*(t + x + y))*std::cos(beta*t);
//				}
//			}
//		}
//		commit(local_server());

		int bs = 2;
		int n = acf_size[0];
		auto identity = [](int){};
		factory::upstream(local_server, this, mapreduce([this](int t) {
			const Index<3> id(acf_size);
			int x1 = acf_size[1];
			int y1 = acf_size[2];
		    for (int x=0; x<x1; x++) {
		        for (int y=0; y<y1; y++) {
					const T k1 = t*delta[0] + x*delta[1] + y*delta[2];
					acf[id(t, x, y)] = gamm*exp(-alpha*k1)*cos(beta*x*delta[1])*cos(beta*t*delta[0]);
//					acf[id(t, x, y)] = gamm*exp(-alpha*k1)*cos(beta*t*delta[0])*cos(beta*x*delta[1])*cos(beta*y*delta[2]);
//					acf[id(t, x, y)] = gamm*exp(-alpha*k1)*cos(-beta*t*delta[0] + beta*x*delta[1] + beta*y*delta[2]);
				}
			}
		}, identity, 0, n, bs));
	}

	void react(factory::Kernel*) override {
		factory::commit(local_server, this);
	}

private:
	const T alpha;
    const T beta;
	const T gamm;
	const Vector<T, 3>& delta;
	const size3& acf_size;
	std::valarray<T>& acf;

	const T G = 9.8;
	const T PI = std::acos(T(-1));
};

//template<class T>
//struct ACF_sub: public Kernel {
//	ACF_sub(ACF<T>& pr, std::size_t tt):
//		p(pr), t(tt) {}
//
//	void act() {
//		const Index<3> id(p.acf_size);
//    	for (std::size_t x=0; x<p.acf_size[1]; x++) {
//    	    for (std::size_t y=0; y<p.acf_size[2]; y++) {
//				const T k1 = t*p.delta[0] + x*p.delta[1] + y*p.delta[2];
//				p.acf[id(t, x, y)] = p.gamm*exp(-p.alpha*k1)*cos(p.beta*t*p.delta[0])*cos(p.beta*x*p.delta[1]);//*cos(beta*y*delta[2]);
//			}
//		}
//		commit(local_server());
//	}
//private:
//	ACF<T>& p;
//	std::size_t t;
//};


template<class T>
struct Solve_Yule_Walker: public Kernel {

	Solve_Yule_Walker(std::valarray<T>& ar_coefs2, std::valarray<T>& aa, std::valarray<T>& bb, const size3& acf_size):
		ar_coefs(ar_coefs2), a(aa), b(bb), _acf_size(acf_size)
	{}

	/*
	void
	solve_linear_system(std::valarray<T>& a, std::valarray<T>& b) {
		using namespace Eigen;
		const int m = ar_coefs.size()-1;
		Matrix<float,Dynamic,Dynamic> A = Matrix<float,Dynamic,Dynamic>::Zero(m, m);
		Matrix<float,Dynamic,1> B = Matrix<float,Dynamic,1>::Zero(m);
		Index<2> idx(size2(m, m));
		for (int i=0; i<m; ++i) {
			for (int j=0; j<m; ++j) {
				A(i, j) = a[idx(j, i)];
			}
			B[i] = b[i];
		}
		Matrix<float,Dynamic,1> x;
		x = A.lu().solve(B);
//		x = A.marked<UpperTriangular>().solveTriangular(B);
		for (int i=0; i<m; ++i) {
			b[i] = x[i];
		}
	}
	*/

	void act() {

		int m = ar_coefs.size()-1;
//		solve_linear_system(a, b);
//		gaussian_elimintation(&a[0], &b[0], m);
		cholesky(&a[0], &b[0], m);
//		sysv<T>('U', m, 1, &a[0], m, &b[0], m, &info);
//		if (info != 0) {
//			std::stringstream s;
//			s << "ssysv error, D(" << info << ", " << info << ")=0";
//			throw std::invalid_argument(s.str());
//		}

		std::copy(&b[0], &b[m], &ar_coefs[1]);
		ar_coefs[0] = 0;

		if (!is_stationary(ar_coefs)) {
			std::stringstream msg;
			msg << "Process is not stationary: |f(i)| > 1\n";
//			int n = ar_coefs.size();
			Index<3> idx(_acf_size);
			for (size_t i=0; i<_acf_size[0]; ++i)
				for (size_t j=0; j<_acf_size[1]; ++j)
					for (size_t k=0; k<_acf_size[2]; ++k)
						if (std::abs(ar_coefs[idx(i, j, k)]) > T(1))
							msg << "ar_coefs[" << i << ',' << j << ',' << k << "] = " << ar_coefs[idx(i, j, k)] << '\n';
			throw std::runtime_error(msg.str());
//			std::cerr << "Continue anyway? y/N\n";
//			char answer = 'n';
//			std::cin >> answer;
//			if (answer == 'n' || answer == 'N') {
//				throw std::runtime_error("Process is not stationary: |f[i]| >= 1.");
//			}
		}
		factory::commit(local_server, this);
	}

private:
	std::valarray<T>& ar_coefs;
	std::valarray<T>& a;
	std::valarray<T>& b;
	const size3& _acf_size;
};

template<class T>
struct Autoreg_coefs: public Kernel {
	Autoreg_coefs(const std::valarray<T>& acf_model_,
				   const size3& acf_size_,
				   std::valarray<T>& ar_coefs_):
		acf_model(acf_model_), acf_size(acf_size_), ar_coefs(ar_coefs_),
		a((ar_coefs.size()-1)*(ar_coefs.size()-1)), b(ar_coefs.size()-1),
		state(0)
	{}

	void act() override {
		factory::upstream(local_server, this, new Yule_walker<T>(acf_model, acf_size, a, b));
	}

	void react(factory::Kernel*) override {
		state++;
		if (state == 1) {
			factory::upstream(local_server, this, new Solve_Yule_Walker<T>(ar_coefs, a, b, acf_size));
		} else {
			factory::commit(local_server, this);
		}
	}

private:
	const std::valarray<T>& acf_model;
	const size3& acf_size;
	std::valarray<T>& ar_coefs;
	std::valarray<T> a;
	std::valarray<T> b;
	int state;
};

}
namespace {

using namespace autoreg;

/**
Generate white noise using Mersenne Twister PRNG and transform it
to normal distribution with variance @var_eps using Box-Muller transform.
*/
template<class T>
void
generate_white_noise(
	std::valarray<T>& eps,
	const T var_eps,
	const Surface_part& p
) {
	if (var_eps < T(0)) {
		throw std::runtime_error("variance is less than zero");
	}

	autoreg::parallel_mt generator(p.part());
	#if !defined(DISABLE_RANDOM_SEED)
	generator.seed(std::chrono::steady_clock::now().time_since_epoch().count());
	#endif
	std::normal_distribution<T> normal(T(0), var_eps);
	std::generate(std::begin(eps), std::end(eps), std::bind(normal, generator));
	if (std::any_of(std::begin(eps), std::end(eps), &stdx::isnan<T>)) {
		throw std::runtime_error("white noise generator produced some NaNs");
	}
}

/// Генерация отдельных частей реализации волновой поверхности.
template<class T>
void generate_zeta(std::valarray<T> phi,
				   const size3& fsize,
				   const Surface_part& p,
				   const std::size_t interval,
				   const size3& zsize,
				   std::valarray<T>& zeta)
{
//	transpose(phi, fsize);
	const Index<3> id(fsize);
	const Index<3> idz(zsize);
	const std::size_t t0 = p.t0();
	const std::size_t t1 = p.t1() - interval;
	const std::size_t x1 = zsize[1];
	const std::size_t y1 = zsize[2];
    for (std::size_t t=0; t<t1-t0; t++) {
        for (std::size_t x=0; x<x1; x++) {
            for (std::size_t y=0; y<y1; y++) {
                const std::size_t m1 = std::min<size_t>(t+1, fsize[0]);
                const std::size_t m2 = std::min<size_t>(x+1, fsize[1]);
                const std::size_t m3 = std::min<size_t>(y+1, fsize[2]);
                T sum = 0;
                for (std::size_t k=0; k<m1; k++)
                    for (std::size_t i=0; i<m2; i++)
                        for (std::size_t j=0; j<m3; j++)
                            sum += phi[id(k, i, j)]*zeta[idz(t-k, x-i, y-j)];
                zeta[idz(t, x, y)] += sum;

				/*
				const size3 delta(m1, m2, m3);
				const size3 offset = size3(t, x, y) - delta;
				std::gslice zeta_slice{
					product(offset),
					{delta[0], delta[1], delta[2]},
					{1, 1, 1}
				};
                zeta[idz(t, x, y)] += (phi * zeta[zeta_slice]).sum();
				*/
            }
        }
    }
}


/// Сшивание частей реализации волновой поверхности.
template<class T>
void weave(const std::valarray<T>& phi,
           const size3& fsize,
           std::valarray<T>& zeta,
           const size3& zsize,
           const Surface_part& p,
           const std::size_t interval)
{
	const Index<3> id(fsize);
	const Index<3> idz(zsize);
    const std::size_t t0 = p.t1() - interval;       // interval left margin
    const std::size_t t1 = p.t1();                  // interval right margin
    const std::size_t x1 = zsize[1];
    const std::size_t y1 = zsize[2];
    for (std::size_t t=0; t<t1-t0; t++) {
        for (std::size_t x=0; x<x1; x++) {
            for (std::size_t y=0; y<y1; y++) {
                // compute left sum
                const std::size_t m1 = std::min<size_t>(fsize[0], t+1);
                const std::size_t m2 = std::min<size_t>(fsize[1], x+1);
                const std::size_t m3 = std::min<size_t>(fsize[2], y+1);
                T sum = 0;
                for (std::size_t k=0; k<m1; k++)
                    for (std::size_t i=0; i<m2; i++)
                        for (std::size_t j=0; j<m3; j++)
                            sum += phi[id(k, i, j)]*zeta[idz(t-k, x-i, y-j)];
                zeta[idz(t, x, y)] += sum;
            }
        }
        for (std::size_t ix=0; ix<x1; ix++) {
            for (std::size_t iy=0; iy<y1; iy++) {
                const std::size_t x = x1-1-ix;
                const std::size_t y = y1-1-iy;
                const std::size_t skip = t1-t+1;
                const std::size_t m1 = std::min<size_t>(fsize[0], zsize[0]-t);
                const std::size_t m2 = std::min<size_t>(fsize[1], zsize[1]-x);
                const std::size_t m3 = std::min<size_t>(fsize[2], zsize[2]-y);
                // compute right sum
                T sum = 0;
                for (std::size_t k=skip; k<m1; k++)
                    for (std::size_t i=0; i<m2; i++)
                        for (std::size_t j=0; j<m3; j++)
                            sum += phi[id(k, i, j)]*zeta[idz(t+k, x+i, y+j)];
                zeta[idz(t, x, y)] += sum;
            }
        }
    }
}

/// Удаление участков разгона из реализации.
template<class T>
void trim_zeta(
	const std::valarray<T>& zeta2,
	const size3& zsize2,
	const size3& zsize,
	std::valarray<T>& zeta
) {
	const size3 delta = zsize2-zsize;
	const std::gslice end_part{
		product(delta),
		{delta[0], delta[1], delta[2]},
		{1, 1, 1}
	};
	zeta = zeta2[end_part];
}


}

namespace autoreg {

template<class T, class Grid>
struct Generator1: public Kernel {

	Generator1() = default;

	Generator1(Surface_part part_, Surface_part part2_,
		const std::valarray<T>& phi_,
	   const size3& fsize_,
	  const T var_eps_,
	  const size3& zsize2_,
	  const std::size_t interval_,
	  const size3& zsize_,
	  const Grid& grid_2_
	): part(part_), part2(part2_),
	phi(phi_), fsize(fsize_), var_eps(var_eps_),
	zsize2(zsize2_), interval(interval_), zsize(zsize_),
	grid_2(grid_2_), left_neighbour(nullptr), count(0)
	{}

	~Generator1() {}

	void set_neighbour(Generator1<T, Grid>* neighbour) {
		left_neighbour = neighbour;
	}

	const Surface_part& get_part() const { return part; }

	void act() override {
		if (_writefile) {
			write_part_to_file(zeta);
			commit(remote_server, this);
		} else {
			#ifndef NDEBUG
			stdx::debug_message("autoreg", "generating part _", part2);
			#endif
			const size3 part_size(part.part_size(), zsize[1], zsize[2]);
			const size3 part_size2(part2.part_size(), zsize2[1], zsize2[2]);
			zsize = part_size;
			zsize2 = part_size2;
			std::valarray<T> zeta2;
			try {
				zeta.resize(zsize);
				zeta2.resize(zsize2);
			} catch (std::exception& x) {
				#ifndef NDEBUG
				stdx::debug_message("autoreg", "resize failed _", stdx::make_object(
					"zsize", zsize,
					"zsize2", zsize2,
					"fsize", fsize,
					"part1", part,
					"part2", part2
				));
				#endif
			}
//			cout << "compute part = " << part.part() << endl;
			generate_white_noise(zeta2, var_eps, part2);
			generate_zeta(phi, fsize, part2, interval, zsize2, zeta2);
			// TODO 2016-02-26 weaving is disabled for benchmarks
			if (not _noweave) {
				if (left_neighbour != nullptr) {
//					cout << "combine part = " << left_neighbour->part.part() << " and "  << part.part() << endl;
					Note* note = new Note;
					note->return_to(left_neighbour);
					compute(note);
//					downstream(local_server(), new Note(), left_neighbour);
				}
				send(local_server, this, this);
//				downstream(local_server(), this, this);
			} else {
				trim_zeta(zeta2, zsize2, zsize, zeta);
				_writefile = true;
				local_server.send(this);
			}
		}
	}

	void react(factory::Kernel*) override {
		count++;
		// received two kernels or last part
//		if (count == 2 || (count == 1 && part.part() == grid_2.num_parts() - 1)) {
////			cout << "release part = " << part.part() << endl;
//			weave(phi, fsize, zeta2, zsize2, part2, interval);
//			trim_zeta(zeta2, zsize2, part, part2, zsize, zeta);
////			commit(local_server());
////			commit(the_io_server() == nullptr ? local_server() : the_io_server());
////			downstream(local_server(), this, parent());
//			commit(remote_server());
//		}
	}

	void
	write_part_to_file(const std::valarray<T>& zeta) {
		std::stringstream filename;
		filename << "zeta-" << std::setw(2) << std::setfill('0') << part.part();
		std::ofstream out(filename.str());
		std::copy(
			std::begin(zeta), std::end(zeta),
			std::ostream_iterator<T>(out, "\n")
		);
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << part << part2;
		out << phi << fsize;
		out << var_eps;
		out << zsize2;
		out << interval;
		out << zsize;
		out << grid_2;
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> part >> part2;
		in >> phi >> fsize;
		in >> var_eps;
		in >> zsize2;
		in >> interval;
		in >> zsize;
		in >> grid_2;
	}

private:

	struct Note: public factory::Kernel {};

	Surface_part part, part2;
	std::valarray<T> phi;
	size3 fsize;
	T var_eps;
	size3 zsize2;
	uint32_t interval;
	size3 zsize;
	Grid grid_2;
	Generator1<T, Grid>* left_neighbour;
	uint32_t count;
	std::valarray<T> zeta;
	bool _writefile = false;
	static const bool _noweave = true;
};

template<class T, class Grid>
struct Wave_surface_generator: public Kernel {

	Wave_surface_generator() = default;

	Wave_surface_generator(const std::valarray<T>& phi_,
						   const size3& fsize_,
						   const T var_eps_,
						   const size3& zsize2_,
						   const std::size_t interval_,
						   const size3& zsize_,
						   const Vector<T, 3>& /*zdelta*/,
						   const Grid& grid_,
						   const Grid& grid_2_):
	phi(phi_), fsize(fsize_), var_eps(var_eps_),
	zsize2(zsize2_), interval(interval_), zsize(zsize_),
	grid(grid_), grid_2(grid_2_), count(0)
	{
//		if (!out.is_open()) throw File_not_found(OUTPUT_FILENAME);
//		out << Domain<T, 3>(zdelta, zsize) << std::endl;
	}

	void
	act() override {
		std::size_t num_parts = grid.num_parts();
		#ifndef NDEBUG
		stdx::debug_message("autoreg", "no. of parts = _", num_parts);
		#endif
		std::vector<Generator1<T, Grid>*> generators(num_parts);
		std::size_t sum = 0;
		for (std::size_t i=0; i<num_parts; ++i) {
			Surface_part part = grid.part(i);
			Surface_part part2 = grid_2.part(i);
			#ifndef NDEBUG
			stdx::debug_message("autoreg", "part #_ = _", i, part);
			#endif
			generators[i] = new Generator1<T, Grid>(part, part2, phi, fsize, var_eps, zsize2, interval, zsize, grid_2);
			sum += part.part_size();
		}
		assert(sum - zsize[0] == 0);
		for (std::size_t i=1; i<num_parts; ++i) {
			generators[i]->set_neighbour(generators[i-1]);
		}
		for (std::size_t i=0; i<num_parts; ++i) {
			factory::upstream(remote_server, this, generators[i]);
		}
	}

	void react(Kernel* child) override {
		#ifndef NDEBUG
		stdx::debug_message(
			"autoreg",
			"generator returned from _, completed _ of _",
			child->from(), count+1, grid.num_parts()
		);
		#endif
		if (++count == grid.num_parts()) {
			commit(remote_server, this);
		}
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << phi;
		out << fsize;
		out << var_eps;
		out << zsize2;
		out << interval;
		out << zsize;
		out << grid;
		out << grid_2;
		out << count;
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> phi;
		in >> fsize;
		in >> var_eps;
		in >> zsize2;
		in >> interval;
		in >> zsize;
		in >> grid;
		in >> grid_2;
		in >> count;
	}

//	void write(const Surface_part& part) {
//		const Index<3> idz(zsize);
//		const int t0 = part.t0();
//		const int t1 = part.t1();
//		const int x1 = zsize[1];
//		const int y1 = zsize[2];
////		std::ofstream slice("zeta_slice.csv");
////		for (int t=t0; t<t1; t++) {
////			for (int x=0; x<x1; x++) {
////				for (int y=0; y<y1; y++) {
////					slice << x/(x1-1.) << ',';
////					slice << y/(y1-1.) << ',';
////					slice << t << ',';
////					slice << zeta[idz(t, x, y)] << '\n';
////				}
////			}
////		}
//	    for (int t=t0; t<t1; t++) {
//	        for (int x=0; x<x1; x++) {
//	            for (int y=0; y<y1; y++) {
//					out.write((char*) &zeta[idz(t, x, y)], sizeof(T));
////					out2 << zeta[idz(t, x, y)] << ' ';
//				}
////				out2 << '\n';
//			}
//		}
//	}

private:

	std::valarray<T> phi;
	size3 fsize;
	T var_eps;
	size3 zsize2;
	uint32_t interval;
	size3 zsize;
	Grid grid;
	Grid grid_2;
	uint32_t count;

};

}

#endif // APPS_AUTOREG_AUTOREG_HH
