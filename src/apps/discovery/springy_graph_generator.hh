#ifndef APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
#define APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH

#include <chrono>

#include <sys/endpoint.hh>

namespace springy {

	struct Node {
		explicit Node(const sys::endpoint& rhs): addr(rhs) {}
		friend std::ostream& operator<<(std::ostream& out, const Node& rhs) {
			return out << "n" << uint64_t(rhs.addr.address()) * uint64_t(rhs.addr.port());
		}
	private:
		const sys::endpoint& addr;
	};

	struct Edge {
		Edge(const sys::endpoint& a, const sys::endpoint& b): x(a), y(b) {}
		friend std::ostream& operator<<(std::ostream& out, const Edge& rhs) {
			return out << Node(rhs.x) << '_' << Node(rhs.y);
		}
	private:
		const sys::endpoint& x;
		const sys::endpoint& y;
	};

	struct Springy_graph {

		typedef std::chrono::nanoseconds::rep Time;
		typedef std::chrono::system_clock clock_type;
		typedef std::chrono::system_clock::time_point time_point;

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
		add_edge(sys::endpoint addr, sys::endpoint principal_addr) {
			#ifndef NDEBUG
			Edge edge(addr, principal_addr);
			Node from(addr);
			Node to(principal_addr);
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
		remove_edge(sys::endpoint addr, sys::endpoint principal_addr) {
			#ifndef NDEBUG
			Edge edge(addr, principal_addr);
			Node from(addr);
			Node to(principal_addr);
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
		add_node(sys::endpoint addr) {
			#ifndef NDEBUG
			Node node(addr);
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
				std::bind(&Springy_graph::add_node, this, _1)
			);
		}

		void
		push_back(const sys::endpoint& rhs) {
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
