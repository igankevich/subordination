namespace factory {

	namespace components {

		template<class Node, class Weight>
		struct Basic_edge {

			friend std::istream operator>>(std::istream& in, Basic_edge& rhs) {
				return in >> rhs.a >> rhs.b >> rhs.w;
			}

			friend std::ostream operator<<(std::ostream& out, const Basic_edge& rhs) {
				return out << rhs.a << ' ' << rhs.b << ' ' << rhs.w;
			}

			Node first() const { return a; }
			Node second() const { return b; }

		private:
			Node a;
			Node b;
			Weight w;
		};

		template<class Node, class Weight>
		struct Shortest_paths {

			typedef Basic_edge<Node, Weight> Edge;

			explicit Shortest_paths(const std::vector<Edge>& e):
				edges(e)
			{ generate_map(); }

			template<class Weight_map>
			void operator()(Weight_map& weights) {
				std::set<Node> uniq_nodes;
				for (auto pair : nodes) {
					uniq_nodes.insert(pair.first);
				}
				for (Node a : uniq_nodes) {
					shortest_path(a, uniq_nodes, weights);
				}
			}

		private:

			template<class Weight_map>
			void shortest_path(Node a, std::set<Node>& nodes, Weight_map& weights) {
				for (Node b : adjacent_nodes(a)) {
					nodes.erase(b);
				}
			}

			std::vector<Node> adjacent_nodes(Node a) const {
				auto range = nodes.equal_range(a);
				std::vector<Node> nds;
				std::copy(range.first, range.second, nds);
				return nds;
			}

			void generate_map() {
				nodes.clear();
				for (Edge edge : edges) {
					nodes.insert({edge.first(), edge.second()});
					nodes.insert({edge.second(), edge.first()});
				}
			}

			const std::vector<Edge>& edges;
			std::multimap<Node, Node> nodes;
		};

		template<class Node, class Weight>
		struct Basic_topology {

			typedef Basic_edge<Node, Weight> Edge;
			typedef Shortest_paths<Node, Weight> Shortest_path;

			friend std::istream operator>>(std::istream& in, Basic_topology& rhs) {
				rhs.read_edges(in);
				rhs.generate_weights();
				return in;
			}

			friend std::ostream operator<<(std::ostream& out, const Basic_topology& rhs) {
				rhs.write_edges(out);
				return out;
			}

		private:

			void generate_weights() {
				Shortest_path path(edges);
				path(weights);
			}

			void read_edges(std::istream& in) {
				edges.clear();
				std::istream_iterator<Edge> it(in), eof;
				std::copy(it, eof, std::back_inserter(edges));
			}

			void write_edges(std::ostream& out) {
				std::ostream_iterator<Edge> it(out, "\n");
				std::copy(edges.begin(), edges.end(), it);
			}

			std::vector<Edge> edges;
			std::map<Node, std::map<Node, Weight>> weights;
		};

	}

}
