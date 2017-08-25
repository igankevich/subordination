#ifndef APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
#define APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH

#include <chrono>
#include <fstream>
#include <functional>
#include <ostream>
#include <unistdx/base/log_message>
#include <cstdint>

namespace springy {

	template<class T>
	struct Node {

		explicit
		Node(const T& rhs) noexcept:
		addr(rhs)
		{}

		const T&
		label() const noexcept {
			return addr;
		}

		float
		mass() const noexcept {
			return 1.f;
		}

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

		Edge(const Node<T>& a, const Node<T>& b) noexcept:
		x(a), y(b)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Edge& rhs) {
			return out << rhs.x << '_' << rhs.y;
		}

	private:

		const Node<T>& x, y;

	};

	template<class T>
	struct Springy_graph {

		typedef std::chrono::system_clock clock_type;
		typedef std::chrono::system_clock::time_point time_point;
		typedef T value_type;
		typedef Node<T> node_type;
		typedef Edge<T> edge_type;

		explicit
		Springy_graph(const char* filename):
		_start(clock_type::now()),
		_outfile(filename),
		_log(_outfile)
		{}

		Springy_graph():
		Springy_graph("springy.log")
		{}

		void
		start() {
			#ifndef NDEBUG
			sys::log_message(_log, "graph", '?', "startTime.push(?);", millis_since_start());
			#endif
		}

		void
		add_edge(const value_type& addr, const value_type& principal_addr) {
			#ifndef NDEBUG
			node_type from(addr);
			node_type to(principal_addr);
			edge_type edge(from, to);
			sys::log_message(_log, "graph", '?',
				"log[logline++] = {"
				"redo: function() {g.? = graph.newEdge(g.?,g.?)},"
				"undo: function() {graph.removeEdge(g.?)},"
				"time: ?"
				"};",
				edge, from, to, edge, millis_since_start()
			);
			#endif
		}

		void
		remove_edge(const value_type& addr, const value_type& principal_addr) {
			#ifndef NDEBUG
			node_type from(addr);
			node_type to(principal_addr);
			edge_type edge(from, to);
			sys::log_message(_log, "graph", '?',
				"log[logline++] = {"
				"redo: function() {graph.removeEdge(g.?)},"
				"undo: function() {g.? = graph.newEdge(g.?,g.?)},"
				"time: ?"
				"};",
				edge, edge, from, to, millis_since_start()
			);
			#endif
		}

		void
		add_node(const value_type& addr) {
			#ifndef NDEBUG
			node_type node(addr);
			sys::log_message(_log, "graph", '?',
				"log[logline++] = {"
				"redo: function() {g.? = graph.newNode({label:'?',mass:?})},"
				"undo: function() {graph.removeNode(g.?)},"
				"time: ?"
				"};",
				node, node.label(), node.mass(), node, millis_since_start()
			);
			#endif
		}

		void
		remove_node(const value_type& addr) {
			#ifndef NDEBUG
			node_type node(addr);
			sys::log_message(_log, "graph", '?',
				"log[logline++] = {"
				"redo: function() {graph.removeNode(g.?)},"
				"undo: function() {g.? = graph.newNode({label:'?',mass:?})},"
				"time: ?"
				"};",
				node, node, node.label(), node.mass(), millis_since_start()
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
		std::ofstream _outfile;
		std::ostream& _log;

	};

	extern springy::Springy_graph<std::uint64_t> graph;

}

#endif // APPS_DISCOVERY_SPRINGY_GRAPH_GENERATOR_HH
