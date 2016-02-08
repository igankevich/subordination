#ifndef APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
#define APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH

#include <chrono>

#include <sysx/endpoint.hh>

namespace springy {

	struct Node {
		explicit Node(const sysx::endpoint& rhs): addr(rhs) {}
		friend std::ostream& operator<<(std::ostream& out, const Node& rhs) {
			return out << "n" << uint64_t(rhs.addr.address()) * uint64_t(rhs.addr.port());
		}
	private:
		const sysx::endpoint& addr;
	};

	struct Edge {
		Edge(const sysx::endpoint& a, const sysx::endpoint& b): x(a), y(b) {}
		friend std::ostream& operator<<(std::ostream& out, const Edge& rhs) {
			return out << Node(rhs.x) << '_' << Node(rhs.y);
		}
	private:
		const sysx::endpoint& x;
		const sysx::endpoint& y;
	};

	struct Springy_graph {

		typedef std::chrono::nanoseconds::rep Time;
		struct graph_log {};
		typedef stdx::log<graph_log> this_log;

		Springy_graph():
		_start(now())
		{}

		void
		start() {
			this_log()
				<< "startTime.push("
				<< now() - _start
				<< "*1e-6);"
				<< std::endl;
		}

		void
		add_edge(sysx::endpoint addr, sysx::endpoint principal_addr) {
			this_log()
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "g." << Edge(addr, principal_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(principal_addr) << ')'
				<< "}, "
				<< "undo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, principal_addr) << ")"
				<< "}}"
				<< ";log[logline-1].time=" << now() - _start << "*1e-6"
				<< std::endl;
		}

		void
		remove_edge(sysx::endpoint addr, sysx::endpoint principal_addr) {
			this_log()
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, principal_addr) << ')'
				<< "}, undo: function() {"
				<< "g." << Edge(addr, principal_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(principal_addr) << ')'
				<< "}}"
				<< ";log[logline-1].time=" << now() - _start << "*1e-6"
				<< std::endl;
		}

		void
		add_node(sysx::endpoint addr) {
			this_log()
				<< "log[logline++] = {"
				<< "redo: function() { g." << Node(addr) << " = graph.newNode({label:'" << addr << "'}) }, "
				<< "undo: function() { graph.removeNode(g." << Node(addr) << ")}}"
				<< ";log[logline-1].time=" << now() - _start << "*1e-6"
				<< std::endl;
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
