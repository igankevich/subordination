#ifndef FACTORY_ID_SET_HH
#define FACTORY_ID_SET_HH

#include <set>
#include <map>
#include <vector>
#include <chrono>
#include <cassert>
#include <algorithm>

#include <stdx/log.hh>

namespace factory {

	namespace components {

		template<class I>
		struct Interval_set {

			typedef Interval<I> interval_type;

			void
			insert(const interval_type& rhs) noexcept {
				auto result = std::find_if(
					_set.begin(), _set.end(),
					[&rhs] (const interval_type& x) {
						return x.overlaps(rhs);
					}
				);
				if (result == _set.end()) {
					_set.insert(rhs);
				} else {
					interval_type acc = rhs;
					auto first = result;
					auto last = _set.end();
					while (first != last && acc.overlaps(*first)) {
						acc.merge(*first);
					}
					const auto it = _set.erase(result, first);
					_set.insert(it, acc);
				}
			}

			template<class Function>
			void
			for_each(Function func) {
				typedef typename interval_type::int_type int_type;
				std::for_each(_set.begin(), _set.end(),
					[&func] (const interval_type& rhs) {
						for (int_type i=rhs.start(); i<rhs.end(); ++i) {
							func(i);
						}
					}
				);
			}

			template<class Set>
			void
			flatten(Set& rhs) const noexcept {
				typedef typename interval_type::int_type int_type;
				this->for_each(
					[&rhs] (int_type i) {
						rhs.insert(i);
					}
				);
			}

		private:

			std::set<interval_type> _set;
		};

	}

}

#endif // FACTORY_ID_SET_HH
