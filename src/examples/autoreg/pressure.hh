#ifndef APPS_AUTOREG_PRESSURE_HH
#define APPS_AUTOREG_PRESSURE_HH

namespace autoreg {

	template<class T>
		using Valarray1D = std::valarray<T>;

	template<class T>
		using Valarray2D = std::valarray<std::valarray<T>>;

	template<class T>
		using Valarray3D = std::valarray<std::valarray<std::valarray<T>>>;

	template<class T>
		using Complex_valarray_2d = std::valarray<std::valarray<std::complex<T>>>;


	template<class T>
	class Velocity_potential: public Kernel {
	public:

		typedef Vector<T, 3> Vec3;

		Velocity_potential(const std::valarray<T>& zeta, const size3& size, const Vector<T, 3>& delta):
			_zeta(zeta), _size(size), _delta(delta), _potential(Valarray2D<Vec3>(Valarray1D<Vec3>(size[2]), size[1]), size[0]) {}

		void act() {
			velocity_potential();
			commit<Local>(this);
		}

		const Valarray3D<Vec3>& potential() const { return _potential; }
		const std::valarray<T>& zeta() const { return _zeta; }

	private:
		std::valarray<T> slope(const std::valarray<T>& surface,
						const size3& size,
						const Vector<T, 3>& zdelta,
						const size_t dimension)
		{

			const T delta = zdelta[dimension];
			std::valarray<T> slopes(size);
			const Index<3> id(size);
			for (size_t i=0; i<size[0]; ++i) {
				for (size_t j=0; j<size[1]; ++j) {
					for (size_t k=0; k<size[2]; ++k) {
						const size3 idx(i, j, k);
						size3 idx1 = idx;
						size3 idx2 = idx;
						if (idx[dimension] == 0) {
							idx1[dimension] += 1;
							idx2[dimension] += 2;
							slopes[id(i, j, k)] = (-T(3)*surface[id(idx)] + T(4)*surface[id(idx1)] + surface[id(idx2)]) / (delta + delta);
						} else if (idx[dimension] == size[dimension]-1) {
							idx1[dimension] -= 1;
							idx2[dimension] -= 2;
							slopes[id(i, j, k)] = (T(3)*surface[id(idx)] - T(4)*surface[id(idx1)] + surface[id(idx2)]) / (delta + delta);
						} else if (idx[dimension] == 1 || idx[dimension] == size[dimension]-2) {
							idx1[dimension] += 1;
							idx2[dimension] -= 1;
							slopes[id(i, j, k)] = (surface[id(idx1)] - surface[id(idx2)]) / (delta + delta);
						} else {
							size3 idx3 = idx;
							size3 idx4 = idx;
							idx1[dimension] -= 2;
							idx2[dimension] -= 1;
							idx3[dimension] += 1;
							idx4[dimension] += 2;
							T c1 = T(1) / T(12);
							T c2 = T(2) / T(3);
							slopes[id(i, j, k)] = (c1*surface[id(idx1)] - c2*surface[id(idx2)] + c2*surface[id(idx3)] - c1*surface[id(idx4)]) / delta;
						}
					}
				}
			}
			return slopes;
		}

		Valarray3D<T> convert(const std::valarray<T>& x, const size3& n) {
			Index<3> idx(n);
			Valarray3D<T> y(Valarray2D<T>(Valarray1D<T>(n[2]), n[1]), n[0]);
			int nt = n[0];
			int nx = n[1];
			int ny = n[2];
			for (int i=0; i<nt; ++i)
				for (int j=0; j<nx; ++j)
					for (int k=0; k<ny; ++k)
						y[i][j][k] = x[idx(i, j, k)];
			return y;
		}

		static void check_array(const Valarray2D<T>& x, int n1, int n2, const char* name) {
			for (int j=0; j<n1; ++j) {
				for (int k=0; k<n2; ++k) {
					if (!std::isfinite(x[j][k])) {
						std::cout << name << " is not finite at " << j << "," << k << std::endl;
					}
				}
			}
		}

		static void check_array(const Complex_valarray_2d<T>& x, int n1, int n2, const char* name) {
			for (int j=0; j<n1; ++j) {
				for (int k=0; k<n2; ++k) {
					if (!std::isfinite(x[j][k].real())) {
						std::cout << name << " (real) is not finite at " << j << "," << k << std::endl;
					}
					if (!std::isfinite(x[j][k].imag())) {
						std::cout << name << " (imag) is not finite at " << j << "," << k << std::endl;
					}
				}
			}
		}

		void velocity_potential() {

			std::valarray<T> _zeta_t = slope(_zeta, _size, _delta, 0);
			std::valarray<T> _zeta_x = slope(_zeta, _size, _delta, 1);
			std::valarray<T> _zeta_y = slope(_zeta, _size, _delta, 2);

			const int nt = _size[0];
			const int nx = _size[1];
			const int ny = _size[2];

			const int np = nx;
			const int nq = ny;

			const T N = _delta[1] * (nx - 1);
			const T dx = T(1) / (nx - 1);
			const T dy = T(1) / (ny - 1);

			Valarray3D<T> zeta_1 = convert(_zeta_t, _size);
			Complex_valarray_2d<T> f_zeta_1(std::valarray<std::complex<T>>(ny), nx);
			Complex_valarray_2d<T> zeta_2(std::valarray<std::complex<T>>(ny), nx);
			Complex_valarray_2d<T> f_zeta_2(std::valarray<std::complex<T>>(ny), nx);
			Complex_valarray_2d<T> tmp2(std::valarray<std::complex<T>>(np), nq);

			T wx0 = 0.0;
			T wx1 = 1.5;
			T wy0 = 0.0;
			T wy1 = 1.5;
			T dw1 = (wx1-wx0) / nx;
			T dw2 = (wy1-wy0) / ny;
//			T r0 = 0.0;
//			T r1 = 1.0;
//			T dr = (r1-r0)/np;
//			T theta0 = 0;
//			T theta1 = _2PI;
//			T dtheta = (theta1-theta0)/nq;

			Index<3> idx(_size);

			Fourier_transform<T, Forward<T>, 2> inner_fft(nx, ny);
			Fourier_transform<T, Backward<T>, 2> outer_fft(nx, ny);

			for (int i=0; i<nt; ++i) {

				inner_fft(zeta_1[i], f_zeta_1);
				check_array(f_zeta_1, nx, ny, "f_zeta_1");

				for (int j=0; j<nx; ++j) {
					for (int k=0; k<ny; ++k) {

						for (int a=0; a<nx; ++a) {
							for (int b=0; b<ny; ++b) {

								T x = dx*a;
								T y = dy*b;

//								T r = r0 + dr*a;
//								T theta = theta0 + dtheta*b;
//								T cos_theta = std::cos(theta);
//								T sin_theta = std::sin(theta);

//								T w1 = wx0 + dw1*a;
//								T w2 = wy0 + dw2*b;
//								T r = std::sqrt(w1*w1 + w2*w2);
//								T cos_theta = a == 0 ? T(0) : w1 / r;
//								T sin_theta = b == 0 ? T(0) : w2 / r;

								T r = std::sqrt(x*x + y*y);
								T cos_theta = a == 0 ? T(0) : x / r;
								T sin_theta = b == 0 ? T(0) : y / r;

								T zeta = _zeta[idx(i, a, b)];
								T zeta_x = _zeta_x[idx(i, a, b)];
								T zeta_y = _zeta_y[idx(i, a, b)];
//								T zeta_t = _zeta_t[idx(i, a, b)];

								std::complex<T> exponent = std::exp(std::complex<T>(r*zeta, x*N));
								std::complex<T> sum = 0;
//								sum += (zeta_x*(T(1)-zeta_x) + zeta_y*(T(1)-zeta_y)) * exponent;
								sum -= std::complex<T>(T(0), zeta_x * cos_theta) * exponent;
								sum -= std::complex<T>(T(0), zeta_y * sin_theta) * exponent;

								if (!std::isfinite(r)) { std::cout << "r is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(cos_theta)) { std::cout << "cos is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(sin_theta)) { std::cout << "sin is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(zeta)) { std::cout << "zeta is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(zeta_x)) { std::cout << "zeta_x is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(zeta_y)) { std::cout << "zeta_y is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(exponent.real())) { std::cout << "exp_real is not finite at " << a << "," << b << std::endl; }
								if (!std::isfinite(exponent.imag())) { std::cout << "exp_imag is not finite at " << a << "," << b << std::endl; }

								zeta_2[a][b] = sum;
							}
						}
						check_array(zeta_2, nx, ny, "zeta_2");

						inner_fft(zeta_2, f_zeta_2);
						Complex_valarray_2d<T> tmp = f_zeta_1 / f_zeta_2;
						outer_fft(tmp, tmp2);

						check_array(f_zeta_2, nx, ny, "f_zeta_2");
						check_array(tmp, nx, ny, "tmp");

						T x = _delta[1]*j;
						T y = _delta[2]*k;
						T zeta = _zeta[idx(i, j, k)];
						T zeta_x = _zeta_x[idx(i, j, k)];
						T sum_phi = 0;
						T sum_phi_x = 0;
						T sum_phi_y = 0;
						T sum_phi_z = 0;

						for (int p=0; p<np; ++p) {
							for (int q=0; q<nq; ++q) {

								T w1 = wx0 + dw1*p;
								T w2 = wy0 + dw2*q;
								T r = std::sqrt(w1*w1 + w2*w2);
								T cos_theta = p == 0 ? T(0) : w1 / r;
								T sin_theta = p == 0 ? T(0) : w2 / r;

								std::complex<T> phi = tmp2[p][q] / _2PI
									* std::complex<T>(zeta_x, cos_theta)
									* std::exp(std::complex<T>(r*zeta, x*w1 + y*w2));

								sum_phi += T(2)*std::real(phi);
								sum_phi_x += T(2)*std::real(phi * std::complex<T>(T(0), r*cos_theta));
								sum_phi_y += T(2)*std::real(phi * std::complex<T>(T(0), r*sin_theta));
								sum_phi_z += T(2)*std::real(phi * r);

//								T r = r0 + dr*p;
//								T theta = theta0 + dtheta*q;
//
//								sum_phi += T(2)*std::real(tmp2[p][q] / _2PI
//									* std::complex<T>(zeta_x, r*std::cos(theta))
//									* std::exp(r * std::complex<T>(zeta, x*std::cos(theta) + y*std::sin(theta))));
							}
						}

						sum_phi *= dw1;
						sum_phi *= dw2;
						sum_phi_x *= dw1;
						sum_phi_x *= dw2;
						sum_phi_y *= dw1;
						sum_phi_y *= dw2;
						sum_phi_z *= dw1;
						sum_phi_z *= dw2;
//						sum_phi *= dr;
//						sum_phi *= dtheta;
						_potential[i][j][k] = Vector<T, 3>(sum_phi_x, sum_phi_y, sum_phi_z);
					}
				}
				std::clog << "Finished " << std::setprecision(2) << std::fixed << (float(i+1)/nt)*100.f << '%' << std::endl;
			}
			std::ofstream out("zeta_t", std::ios::binary);
			for (int i=0; i<nt; ++i) {
				for (int j=0; j<nx; ++j) {
					for (int k=0; k<ny; ++k) {
						out.write((char*)&zeta_1[i][j][k], sizeof(T));
					}
				}
			}

		}

		std::valarray<T> _zeta;
		const size3& _size;
		Vector<T, 3> _delta;
		Valarray3D<Vec3> _potential;

		static T _2PI;
		static T PI;
	};

	template<class T>
	T Velocity_potential<T>::_2PI = T(2)*std::acos(T(-1));

	template<class T>
	T Velocity_potential<T>::PI = std::acos(T(-1));

}

#endif // vim:filetype=cpp
