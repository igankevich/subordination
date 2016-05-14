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

		Springy_graph():
		_start(now())
		{}

		void
		start() {
			#ifndef NDEBUG
			stdx::debug_message("graph")
				<< "startTime.push("
				<< now() - _start
				<< "*1e-6);";
			#endif
		}

		void
		add_edge(sys::endpoint addr, sys::endpoint principal_addr) {
			#ifndef NDEBUG
			stdx::debug_message("graph")
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "g." << Edge(addr, principal_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(principal_addr) << ')'
				<< "}, "
				<< "undo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, principal_addr) << ")"
				<< "},"
				<< "time: " << now() - _start << "*1e-6"
				<< "};";
			#endif
		}

		void
		remove_edge(sys::endpoint addr, sys::endpoint principal_addr) {
			#ifndef NDEBUG
			stdx::debug_message("graph")
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, principal_addr) << ')'
				<< "}, undo: function() {"
				<< "g." << Edge(addr, principal_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(principal_addr) << ')'
				<< "},"
				<< "time: " << now() - _start << "*1e-6"
				<< "};";
			#endif
		}

		void
		add_node(sys::endpoint addr) {
			#ifndef NDEBUG
			stdx::debug_message("graph")
				<< "log[logline++] = {"
				<< "redo: function() { g." << Node(addr) << " = graph.newNode({label:'" << addr << "'}) }, "
				<< "undo: function() { graph.removeNode(g." << Node(addr) << ")},"
				<< "time: " << now() - _start << "*1e-6"
				<< "};";
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

		Time
		now() {
			using namespace std::chrono;
			typedef std::chrono::steady_clock Clock;
			return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
		}

		Time _start;

	};

}

#endif // APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
