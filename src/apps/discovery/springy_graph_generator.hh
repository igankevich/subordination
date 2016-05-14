#ifndef APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
#define APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH

#include <chrono>

namespace springy {

	template<class T>
	struct Node {

		explicit
		Node(const T& rhs) noexcept:
		addr(rhs)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Node& rhs) {
			std::hash<T> hsh;
			return out << 'n' << hsh(rhs.addr);
		}

	private:

		const T& addr;

	};

	template<class T>
	struct Edge {

		Edge(const T& a, const T& b) noexcept:
		x(a), y(b)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Edge& rhs) {
			return out << Node<T>(rhs.x) << '_' << Node<T>(rhs.y);
		}

	private:

		const T& x, y;

	};

	template<class T>
	struct Springy_graph {

		typedef std::chrono::system_clock clock_type;
		typedef std::chrono::system_clock::time_point time_point;
		typedef T value_type;
		typedef Node<T> node_type;
		typedef Edge<T> edge_type;

		Springy_graph():
		_start(clock_type::now())
		{}

		void
		start() {
			#ifndef NDEBUG
			stdx::debug_message("graph", '?', "startTime.push(?*1e-6);", millis_since_start());
			#endif
		}

		void
		add_edge(const value_type& addr, const value_type& principal_addr) {
			#ifndef NDEBUG
			edge_type edge(addr, principal_addr);
			node_type from(addr);
			node_type to(principal_addr);
			stdx::debug_message("graph", '?',
				"log[logline++] = {"
				"redo: function() {g.? = graph.newEdge(g.?,g.?)},"
				"undo: function () {graph.removeEdge(g.?)},"
				"time: ?*1e-6"
				"};",
				edge, from, to, edge, millis_since_start()
			);
			#endif
		}

		void
		remove_edge(const value_type& addr, const value_type& principal_addr) {
			#ifndef NDEBUG
			edge_type edge(addr, principal_addr);
			node_type from(addr);
			node_type to(principal_addr);
			stdx::debug_message("graph", '?',
				"log[logline++] = {"
				"redo: function () {graph.removeEdge(g.?)},"
				"undo: function() {g.? = graph.newEdge(g.?,g.?)},"
				"time: ?*1e-6"
				"};",
				edge, edge, from, to, millis_since_start()
			);
			#endif
		}

		void
		add_node(const value_type& addr) {
			#ifndef NDEBUG
			node_type node(addr);
			stdx::debug_message("graph", '?',
				"log[logline++] = {"
				"redo: function () {g.? = graph.newNode({label:'?'})},"
				"undo: function() {graph.removeNode(g.?)},"
				"time: ?*1e-6"
				"};",
				node, addr, node, millis_since_start()
			);
			#endif
		}

		template<class It>
		void
		add_nodes(It first, It last) {
			using namespace std::placeholders;
			std::for_each(
				first, last,
				std::bind(&Springy_graph<T>::add_node, this, _1)
			);
		}

		void
		push_back(const value_type& rhs) {
			add_node(rhs);
		}

	private:

		std::chrono::milliseconds::rep
		millis_since_start() {
			using namespace std::chrono;
			return duration_cast<milliseconds>(clock_type::now() - _start).count();
		}

		time_point _start;

	};

}

#endif // APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
