#ifndef APPS_DISCOVERY_DISTANCE_HH
#define APPS_DISCOVERY_DISTANCE_HH

#include <limits>
#include <utility>

#include <unistdx/net/endpoint>

namespace discovery {

	namespace bits {

		template<class T>
		std::pair<T,T>
		position_in_tree(T offset, T fanout) noexcept {
			T layer = 0;
			T n = 1; // no. of nodes in layer
			while (offset > n) { offset -= n; ++layer; n *= fanout; }
			return std::make_pair(layer, offset-1);
		}

	}

	template<class Addr>
	struct Distance_in_tree {

		typedef Addr addr_type;
		typedef typename addr_type::rep_type rep_type;
		typedef std::pair<rep_type,rep_type> pair_type;

		Distance_in_tree(): _pair() {}

		Distance_in_tree(rep_type fanout, const addr_type& addr, const addr_type& netmask) noexcept:
		_pair(bits::position_in_tree(addr.position(netmask), fanout))
		{}

		Distance_in_tree(rep_type lvl, rep_type off) noexcept:
		_pair(lvl, off)
		{}

		Distance_in_tree(
			const addr_type& from,
			const addr_type& to,
			const addr_type& netmask,
			const rep_type fanout
		) noexcept:
		_pair(
			pair_sub(
				fanout,
				bits::position_in_tree(to.position(netmask), fanout),
				bits::position_in_tree(from.position(netmask), fanout)
			)
		)
		{}

		bool
		operator<(const Distance_in_tree& rhs) const noexcept {
			return _pair < rhs._pair;
		}

		bool
		operator==(const Distance_in_tree& rhs) const noexcept {
			return _pair == rhs._pair;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Distance_in_tree& rhs) {
			return out << rhs._level << ':' << rhs._offset;
		}

	private:

		constexpr static rep_type
		abs_sub(rep_type a, rep_type b) noexcept {
			return a < b ? b-a : a-b;
		}

		constexpr static rep_type
		lvl_sub(rep_type a, rep_type b) noexcept {
			return a > b ? higher_level : (b-a == 0 ? equal_level : b-a);
		}

		constexpr static pair_type
		pair_sub(rep_type fanout, pair_type a, pair_type b) noexcept {
			//const rep_type k = a.first < b.first ? b.first-a.first : 0;
			return std::make_pair(
				lvl_sub(a.first, b.first),
				abs_sub(a.second, b.second/fanout)
			);
		}

		union {
			struct {
				rep_type _level;
				rep_type _offset;
			};
			std::pair<rep_type,rep_type> _pair;
		};

		static constexpr const rep_type equal_level = 10000;
		static constexpr const rep_type higher_level = 20000;
	};

}

#endif // APPS_DISCOVERY_DISTANCE_HH
