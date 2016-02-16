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

		template<class T>
		constexpr T
		to_radians(T degrees) noexcept {
			return degrees * M_PI / T(180);
		}

		/// @see https://www.math.ksu.edu/~dbski/writings/haversine.pdf
		template<class T>
		constexpr T
		haversin(T alpha) noexcept {
			return T(0.5) * (T(1) - std::cos(alpha));
		}

		template<class T>
		constexpr T
		distance_on_sphere(T lat1, T lon1, T lat2, T lon2) noexcept {
			const T h = haversin(lat1-lat2) +
				std::cos(lat1) * std::cos(lat2)
				* haversin(lon1-lon2);
//			const T h = stdx::pow2(std::sin(T(0.5)*(lat2-lat1))) +
//				std::cos(lat1) * std::cos(lat2) *
//				stdx::pow2(std::sin(T(0.5)*(lon2-lon1)));
			return std::asin(std::sqrt(h));
		}

	}

	template<class Float>
	struct Point {

		typedef Float float_type;

		constexpr Point
		operator-(const Point& rhs) const noexcept {
			return Point{_x - rhs._x, _y - rhs._y};
		}

		constexpr Point
		operator*(float_type rhs) const noexcept {
			return Point{_x*rhs, _y*rhs};
		}

		constexpr Point
		operator/(float_type rhs) const noexcept {
			return Point{_x/rhs, _y/rhs};
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Point& rhs) {
			return out << rhs._x << ' ' << rhs._y;
		}

		float_type _x;
		float_type _y;

	};

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

		constexpr Point<float_type>
		point() const noexcept {
			return Point<float_type>{_longitude, _latitude};
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
