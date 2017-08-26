#ifndef APPS_DISCOVERY_HIERARCHY_WITH_GRAPH_HH
#define APPS_DISCOVERY_HIERARCHY_WITH_GRAPH_HH

#include <springy/springy.hh>

namespace discovery {

	template<class Base>
	struct Hierarchy_with_graph: public Base {

		typedef Base base_type;
		using base_type::_bindaddr;
		using base_type::_subordinates;

		template<class ... Args>
		explicit
		Hierarchy_with_graph(Args&& ... args):
		Base(std::forward<Args>(args)...)
		{}

		void
		add_subordinate(const sys::endpoint& addr) {
			if (_subordinates.count(addr) == 0) {
				_graph.add_edge(addr, _bindaddr);
			}
			base_type::add_subordinate(addr);
		}

		void
		remove_subordinate(const sys::endpoint& addr) {
			if (_subordinates.count(addr) > 0) {
				_graph.remove_edge(addr, _bindaddr);
			}
			base_type::remove_subordinate(addr);
		}

	private:

		springy::Springy_graph<sys::endpoint> _graph;

	};

}

#endif // vim:filetype=cpp
