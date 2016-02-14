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
			return _id < rhs._id;
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
		return std::sqrt(
			stdx::pow2(lhs._latitude - rhs._latitude)
			+
			stdx::pow2(lhs._longitude - rhs._longitude)
		)
		* lhs._nhosts
		/ rhs._nhosts;
	}

}

#endif // APPS_DISCOVERY_LOCATION_HH
