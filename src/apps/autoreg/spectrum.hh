#ifndef APPS_AUTOREG_SPECTRUM_HH
#define APPS_AUTOREG_SPECTRUM_HH

namespace autoreg {

	typedef int size_type;


	template<class T>
	struct Const {
		static constexpr T PI = std::acos(T(-1));
	};

	template<class T, class size_type>
	inline T frequency_spec(T omega, T omega_max, size_type n) {
		if (omega == T(0)) return T(0);
		T omega_1 = omega/omega_max;
		T omega_n = std::pow(omega_1, -n);
		return (n/omega_max) * omega_n * std::exp(-(n/(n-T(1))) * omega_1 * omega_n);
	}

	template<class T, class size_type>
	inline T directional_spec(T theta, T theta_max, size_type m) {
		return std::tgamma(m + size_type(1))
			/ (std::pow(2, m) * std::pow(std::tgamma((m+T(1))/T(2)), 2))
			* std::pow(std::cos(theta-theta_max), m);
	}

}

#endif // APPS_AUTOREG_SPECTRUM_HH
