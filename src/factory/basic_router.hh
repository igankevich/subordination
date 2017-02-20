#ifndef FACTORY_BASIC_ROUTER_HH
#define FACTORY_BASIC_ROUTER_HH

#include <unordered_map>
#include <vector>
#include <cassert>
#include <sstream>
#include <stdx/iterator.hh>
#include <sys/endpoint.hh>

namespace factory {

	template<class Kernel>
	class Basic_router {
		typedef Kernel kernel_type;
		typedef typename Kernel::id_type id_type;
		typedef std::pair<kernel_type*,sys::endpoint> queue_elem;
		typedef std::vector<queue_elem> queue_type;
		typedef typename queue_type::iterator queue_iter;

		std::unordered_map<id_type, queue_type> _subordinates;

	public:
		void
		add_subordinate(kernel_type* k, const sys::endpoint& dest) {
			assert(k != nullptr);
			assert(k->identifiable());
			assert(k->parent() != nullptr);
			assert(k->parent()->identifiable());
			assert(dest);
			const id_type parent_id = k->parent()->id();
			queue_type& neighbours = _subordinates[parent_id];
			auto& nbrs = k->neighbours();
			for (const queue_elem& nbr : neighbours) {
				const sys::endpoint& endp = nbr.second;
				if (std::find(nbrs.begin(), nbrs.end(), endp) == nbrs.end()) {
					nbrs.emplace_back(endp);
				}
			}
			#ifndef NDEBUG
			print_neighbours(k, dest);
			#endif
			neighbours.emplace_back(k, dest);
			#ifndef NDEBUG
			stdx::debug_message(
				"nbrs",
				"save subordinate: _ -> (_,_)",
				parent_id,
				k->id(),
				dest
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
				[k] (const queue_elem& rhs) {
					return k->id() == rhs.first->id();
				}
			);
			if (result != children.end()) {
				#ifndef NDEBUG
				stdx::debug_message(
					"nbrs",
					"erase subordinate: _ -> (_,_)",
					parent_id,
					k->id(),
					result->second
				);
				children.erase(result);
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
		print_neighbours(kernel_type* kernel, const sys::endpoint& dest) {
			#ifndef NDEBUG
			std::stringstream tmp;
			const auto& nbrs = kernel->neighbours();
			std::copy(
				nbrs.begin(),
				nbrs.end(),
				stdx::intersperse_iterator<sys::endpoint,char>(tmp, ',')
			);
			stdx::debug_message(
				"nbrs",
				"_'s neighbours: [_]",
				dest,
				tmp.str()
			);
			#endif
		}

	};

}

#endif // FACTORY_BASIC_ROUTER_HH
