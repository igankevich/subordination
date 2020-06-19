#ifndef DTEST_CLUSTER_NODE_BITMAP_HH
#define DTEST_CLUSTER_NODE_BITMAP_HH

#include <initializer_list>
#include <string>
#include <vector>

namespace dts {

    class cluster_node_bitmap {

    public:
        using bitarray = std::vector<bool>;
        using index_array = std::initializer_list<size_t>;

    private:
        bitarray _nodes;

    public:
        inline explicit cluster_node_bitmap(bitarray nodes): _nodes(std::move(nodes)) {}
        inline explicit cluster_node_bitmap(size_t n, bool b=false): _nodes(n, b) {}
        explicit cluster_node_bitmap(size_t n, index_array indices);
        inline bool matches(size_t node_number) const { return this->_nodes[node_number]; }
        bool intersects(const cluster_node_bitmap& rhs) const;
        inline size_t size() const noexcept { return this->_nodes.size(); }
        void read(std::string arg);
        friend std::ostream& operator<<(std::ostream& out, const cluster_node_bitmap& rhs);
    };

    std::ostream& operator<<(std::ostream& out, const cluster_node_bitmap& rhs);

}

#endif // vim:filetype=cpp
