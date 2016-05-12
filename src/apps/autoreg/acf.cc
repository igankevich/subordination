#include <stdx/debug.hh>

#include <iostream>
#include <iomanip>
#include <cmath>
#include <valarray>
#include <fstream>
#include <sstream>
#include <array>
#include <complex>

#include "spectrum.hh"
#include "fourier.hh"

template<class T>
void least_squares(T P, T p, T A, T b, int n, int N)
{
	for (int k=0; k<n; k++) {
		b[k] = 0;
		for (int j=0; j<n; j++) A[k][j] = 0;
		for (int i=0; i<N; i++) {
			for (int j=0; j<n; j++) {
				A[k][j] += P[i][k]*P[i][j];
//				cout << "A["<<k<<"]["<<j<<"] += P["<<i<<"]["<<k<<"]*P["<<i<<"]["<<j<<"]" << endl;
			}
			b[k] += P[i][k]*p[i];
//			cout << "b["<<k<<"] += P["<<i<<"]["<<k<<"]*p["<<i<<"]" << endl;
		}
	}
}

//typedef float T;
typedef int I;

template<class T, size_t N>
std::ostream& operator<<(std::ostream& out, const std::array<T, N>& rhs) {
	out << rhs[0];
	for (I i=1; i<rhs.size(); ++i)
		out << ',' << rhs[i];
	return out;
}


template<class T, class I>
struct Spectrum_app {
	typedef autoreg::Discrete_function<T, 2> Spectrum;
	typedef autoreg::Discrete_function<T, 3> ACF;
	typedef autoreg::Discrete_function<std::complex<T>, 2> Complex_arr;

	Spectrum_app(int argc, char* argv[]):
		_omega_0(0.0),
		_omega_1(2.5),
		_omega_max(1.02),
		_theta_0(0),
		_theta_1(PI),
		_theta_max(1.5),
		_n(4),
		_m(2),
		_size{64, 64}
	{
		std::stringstream cmdline;
		for (int i=1; i<argc; ++i) {
			cmdline << argv[i] << ' ';
		}
		std::string arg;
		while (cmdline >> arg) {
			if (arg == "--omega-max")      { read_option(cmdline, arg, _omega_max); }
			else if (arg == "--omega-0")   { read_option(cmdline, arg, _omega_0); }
			else if (arg == "--omega-1")   { read_option(cmdline, arg, _omega_1); }
			else if (arg == "--theta-max") { read_option(cmdline, arg, _theta_max); }
			else if (arg == "--theta-0")   { read_option(cmdline, arg, _theta_0); }
			else if (arg == "--theta-1")   { read_option(cmdline, arg, _theta_1); }
			else if (arg == "--n")         { read_option(cmdline, arg, _n); }
			else if (arg == "--m")         { read_option(cmdline, arg, _m); }
//			else if (arg == "--size")      { read_option(cmdline, arg, _size); }
		}
	}

	void run() {

//		Spectrum spec{_size, {_omega_0, _theta_0}, {_omega_1, _theta_1}};
//		spec([this, &spec] (T& val, int idx[2]) {
//			auto pt = spec.domain_point(idx);
//			T omega = pt[0];
//			T theta = pt[1];
//			val = autoreg::frequency_spec(omega, _omega_max, _n)
//				* autoreg::directional_spec(theta, _theta_max, _m);
//		});
//		{
//			std::ofstream out("spec.blob");
//			spec.write(out);
//		}

		Spectrum spec{_size,
			{T(-0.3), T(0)},
			{T( 0.3), T(1.5)}
		};
		spec([this, &spec] (T& val, int idx[2]) {
			auto pt = spec.domain_point(idx);
			T u = pt[0];
			T v = pt[1];
			T disp = std::pow(u*u + v*v, T(0.25));
			T omega = std::sqrt(G) * disp;
			T theta = std::atan2(v, u);
			T jacobian = disp == T(0) ? T(1) : T(0.5) * std::sqrt(G) / std::pow(disp, 3);
//			jacobian = T(2) * std::pow(omega, 3) / G / G;
			jacobian = 1;
			jacobian *= 0.4;
			val = autoreg::frequency_spec(omega, _omega_max, _n)
				* autoreg::directional_spec(theta, _theta_max, _m)
				* jacobian;
		});
		{
			std::ofstream out("spec.blob");
			spec.write(out);
		}

//		Fourier_transform<T, Backward<T>, 2> fft(_size[0], _size[1]);
//		Complex_arr tmp{{_size[0], _size[1]}};
//		fft(spec, tmp);
//		ACF acf{{16, _size[0], _size[1]}};
//		acf([this, &spec, &acf, &tmp] (T& val, int idx[3]) {
//			auto acf_pt = acf.domain_point(idx);
//			T t = acf_pt[2];
//			t = 0;
//			const T alpha = 0.05;
//			const T beta  = 0.8;
//			val = (T(1)/PI)
//				* T(4.0)*std::real(tmp[idx[0]][idx[1]])
//				* std::exp(-alpha*t)*std::cos(beta*t);
////			std::cout << "tmp=" << tmp[idx[0]][idx[1]] << std::endl;
//		});
		ACF acf{{10, 10, 10}};
		auto delta = spec.domain_delta();
		T du = delta[0];
		T dv = delta[1];
		acf([this, &spec, &acf, &du, &dv] (T& val, int idx[3]) {
			auto acf_pt = acf.domain_point(idx);
			T t = acf_pt[2];
			T x = acf_pt[1];
			T y = acf_pt[0];
			std::complex<T> sum = 0;
			spec([this, &spec, &sum, &t, &x, &y, &du, &dv] (T val2, int idx2[2]) {
				auto pt = spec.domain_point(idx2);
//				T u = _2PI * idx2[0] / _size[0];
//				T v = _2PI * idx2[1] / _size[1];
				T u = pt[0];
				T v = pt[1];
				T w = std::sqrt(G*(u*u + v*v));
//				T omega = pt[0];
//				T theta = pt[1];
//				T u = omega*omega*std::cos(theta)/G;
//				T v = omega*omega*std::sin(theta)/G;
////				T w = omega;
//				T w = std::sqrt(G*(u*u + v*v));
//				sum += val2 * std::cos(x*u + y*v - w*t);
				sum += val2 * std::exp(std::complex<T>(T(0), -w*t + x*u + y*v));
//				std::cout << w << " == " << omega << std::endl;
			});
//			const T alpha = 0.05;
//			const T beta  = 0.8;
			val = (T(1)/PI) * std::norm(sum)*du*dv;// * std::exp(-alpha*(t + x + y))*std::cos(beta*t);
		});
		{
			ACF acf{{10, 10, 10}};
			acf([this, &acf] (T& val, int idx[3]) {
				auto pt = acf.domain_point(idx);
				T t = pt[2];
				T x = pt[1];
				T y = pt[0];
				const T alpha = 0.05;
				const T beta  = 0.8;
				const T gamm  = 1.0;
				val = gamm*exp(-alpha*(t + x + y))*cos(beta*t)*cos(beta*x)*cos(beta*y);
			});
			std::ofstream out("acf2.blob");
			acf.write(out);
		}
		{
			std::ofstream out("acf.blob");
			acf.write(out);
		}
		write_acf(std::cout, acf);
	}

	// TODO: old style
	void write_acf(std::ostream& out, ACF& acf) {
		auto size = acf.size();
		out << size[0]
			<< ',' << size[1]
			<< ',' << size[2] << '\n';
		acf([&out] (T  val, int [3]) {
//			out << "val[";
//			out << idx[2] << ',';
//			out << idx[1] << ',';
//			out << idx[0];
//			out << "] = ";
			out << val << '\n';
		});
	}

private:

	template<class X>
	void read_option(std::istream& cmdline, const std::string& option, X& value) {
		cmdline >> value;
		if (!cmdline) {
			std::stringstream msg;
			msg << "Wrong value for option " << option;
			throw std::runtime_error(msg.str());
		}
	}

	T _omega_0;
	T _omega_1;
	T _omega_max;
	T _theta_0;
	T _theta_1;
	T _theta_max;
	I _n;
	I _m;
	typename autoreg::Discrete_function<T,2>::V _size;

	const T G = 9.8;
	const T PI = std::acos(T(-1));
	const T _2PI = std::acos(T(-1)) * T(2);
};

int main(int argc, char* argv[]) {
	Spectrum_app<float, int> app(argc, argv);
	app.run();
	return 0;
}
