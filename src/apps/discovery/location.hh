#ifndef APPS_DISCOVERY_LOCATION_HH
#define APPS_DISCOVERY_LOCATION_HH

#include <cmath>
#include <stdx/log.hh>

namespace stdx {
	template<class T>
	constexpr T
	pow2(T x) noexcept {
		return x*x;
	}
}

namespace discovery {

	namespace bits {

#define TO_RAD (3.1415926536 / 180)
double dist(double th1, double ph1, double th2, double ph2)
{
	double dx, dy, dz;
	ph1 -= ph2;
	ph1 *= TO_RAD, th1 *= TO_RAD, th2 *= TO_RAD;

	dz = sin(th1) - sin(th2);
	dx = cos(ph1) * cos(th1) - cos(th2);
	dy = sin(ph1) * cos(th1);
	return asin(sqrt(dx * dx + dy * dy + dz * dz) / 2);
}

		template<class T>
		constexpr T
		to_radians(T degrees) {
			return degrees * M_PI / T(180);
		}

		/// @see https://www.math.ksu.edu/~dbski/writings/haversine.pdf
		template<class T>
		constexpr T
		haversin(T alpha) {
			return T(0.5) * (T(1) - std::cos(alpha));
		}

		template<class T>
		constexpr T
		distance_on_sphere(T lat1, T lon1, T lat2, T lon2) {
			const T h = haversin(lat1-lat2) +
				std::cos(lat1) * std::cos(lat2)
				* haversin(lon1-lon2);
//			const T h = stdx::pow2(std::sin(T(0.5)*(lat2-lat1))) +
//				std::cos(lat1) * std::cos(lat2) *
//				stdx::pow2(std::sin(T(0.5)*(lon2-lon1)));
			return std::asin(std::sqrt(h));
		}

	}

	struct Location {

		typedef float float_type;
		typedef uint32_t id_type;
		typedef uint32_t int_type;

		constexpr
		Location(id_type id, float_type lat, float_type lon, int_type nhosts) noexcept:
		_id(id),
		_latitude(lat),
		_longitude(lon),
		_nhosts(nhosts)
		{}

		Location() = default;
		Location(const Location&) = default;
		Location& operator=(const Location&) = default;
		Location(Location&&) = default;
		~Location() = default;

		constexpr bool
		operator==(const Location& rhs) const noexcept {
			return _id == rhs._id;
		}

		constexpr bool
		operator!=(const Location& rhs) const noexcept {
			return !operator==(rhs);
		}

		constexpr bool
		operator<(const Location& rhs) const noexcept {
			return _nhosts < rhs._nhosts;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Location& rhs) {
//			return out << rhs._id << " [pos=\"" << rhs._latitude << ',' << rhs._longitude << "!\"];";
			return out << rhs._longitude << ' ' << rhs._latitude;
		}

		void add_hosts(int_type n) {
			_nhosts += n;
		}

		friend constexpr Location::float_type
		distance(const Location& lhs, const Location& rhs) noexcept;

	private:

		id_type _id;
		float_type _latitude;
		float_type _longitude;
		int_type _nhosts;

	};

	constexpr Location::float_type
	distance(const Location& lhs, const Location& rhs) noexcept {
		return bits::distance_on_sphere(
			bits::to_radians(lhs._latitude), bits::to_radians(lhs._longitude),
			bits::to_radians(rhs._latitude), bits::to_radians(rhs._longitude)
		)
//		* (Location::float_type(lhs._nhosts) / Location::float_type(rhs._nhosts))
		;
	}

	struct City {

		typedef float float_type;
		typedef uint32_t id_type;

		constexpr bool
		operator<(const City& rhs) const noexcept {
			return _city < rhs._city;// and _country == rhs._country;
		}

		constexpr bool
		operator==(const City& rhs) const noexcept {
			return _city == rhs._city;
		}

		constexpr bool
		operator!=(const City& rhs) const noexcept {
			return !operator==(rhs);
		}

		id_type _country;
		id_type _city;
		float_type _latitude;
		float_type _longitude;

	};

}

#endif // APPS_DISCOVERY_LOCATION_HH
