#ifndef FACTORY_BASIC_ROUTER_HH
#define FACTORY_BASIC_ROUTER_HH

#include <unordered_map>
#include <vector>
#include <cassert>
#include <sstream>
#include <stdx/iterator.hh>

namespace factory {

	template<class Kernel>
	class Basic_router {
		typedef Kernel kernel_type;
		typedef typename Kernel::id_type id_type;
		typedef std::vector<kernel_type*> queue_type;
		typedef typename queue_type::iterator queue_iter;

		std::unordered_map<id_type, queue_type> _subordinates;

	public:
		void
		add_subordinate(kernel_type* k) {
			assert(k != nullptr);
			assert(k->identifiable());
			assert(k->parent() != nullptr);
			assert(k->parent()->identifiable());
			const id_type parent_id = k->parent()->id();
			queue_type& neighbours = _subordinates[parent_id];
			if (!neighbours.empty()) {
				k->neighbour(neighbours.back());
			}
			#ifndef NDEBUG
			print_neighbours(k);
			#endif
			neighbours.push_back(k);
			#ifndef NDEBUG
			stdx::debug_message(
				"nbrs",
				"save subordinate: _ -> _",
				parent_id,
				k->id()
			);
			#endif
		}

		/*
		kernel_type*
		get_neighbour_of(kernel_type* k) {
			const id_type parent_id = k->parent()->id();
			queue_iter result = _subordinates.find(parent_id);
			return (result == _subordinates.end())
				? nullptr : result->back();
		}
		*/

		void
		erase_subordinate(kernel_type* k) {
			assert(k != nullptr);
			assert(k->identifiable());
			assert(k->parent() != nullptr);
			assert(k->parent()->identifiable());
			id_type parent_id = k->parent()->id();
			queue_type& children = _subordinates[parent_id];
			queue_iter result = std::find_if(
				children.begin(),
				children.end(),
				[k] (const kernel_type* rhs) {
					return k->id() == rhs->id();
				}
			);
			if (result != children.end()) {
				children.erase(result);
				#ifndef NDEBUG
				stdx::debug_message(
					"nbrs",
					"erase subordinate: _ -> _",
					parent_id,
					k->id()
				);
				#endif
				if (children.empty()) {
					#ifndef NDEBUG
					stdx::debug_message("nbrs", "erase principal _ ", parent_id);
					#endif
					_subordinates.erase(parent_id);
				}
			}
		}

	private:

		static void
		print_neighbours(kernel_type* kernel) {
			#ifndef NDEBUG
			std::stringstream tmp;
			stdx::intersperse_iterator<id_type,char> it(tmp, ',');
			kernel_type* neighbour = kernel->neighbour();
			while (neighbour != nullptr) {
				*it++ = neighbour->id();
				neighbour = neighbour->neighbour();
			}
			stdx::debug_message(
				"nbrs",
				"_'s neighbours: [_]",
				kernel->id(),
				tmp.str()
			);
			#endif
		}

	};

}

#endif // FACTORY_BASIC_ROUTER_HH
