#ifndef FACTORY_BASIC_ROUTER_HH
#define FACTORY_BASIC_ROUTER_HH

#include <unordered_map>
#include <vector>

namespace factory {

	template<class Kernel>
	class Basic_router {

	public:
		typedef Kernel kernel_type;
		typedef typename Kernel::id_type id_type;

		void
		add_subordinate(kernel_type* k) {
			_subordinates[k->parent()->id()].push_back(k);
			#ifndef NDEBUG
			stdx::debug_message(
				"tst",
				"save subordinate: _ -> _",
				k->parent()->id(),
				k->id()
			);
			#endif
		}

		void
		erase_subordinate(kernel_type* k) {
			id_type parent_id = k->parent()->id();
			auto& children = _subordinates[parent_id];
			auto result = std::find_if(
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
					"tst",
					"erase subordinate: _ -> _",
					parent_id,
					k->id()
				);
				#endif
				if (children.empty()) {
					#ifndef NDEBUG
					stdx::debug_message("tst", "erase principal _ ", parent_id);
					#endif
					_subordinates.erase(parent_id);
				}
			}
		}

	private:
		std::unordered_map<id_type, std::vector<kernel_type*>> _subordinates;

	};

}

#endif // FACTORY_BASIC_ROUTER_HH
