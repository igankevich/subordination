#ifndef DTEST_CLUSTER_NODE_BITMAP_HH
#define DTEST_CLUSTER_NODE_BITMAP_HH

#include <string>
#include <vector>

namespace dts {


    class cluster_node_bitmap {

    private:
        std::vector<bool> _nodes;

    public:

        inline explicit cluster_node_bitmap(size_t n): _nodes(n, false) {}
        inline bool matches(size_t node_number) const { return this->_nodes[node_number]; }
        void read(std::string arg);

    };

}

#endif // vim:filetype=cpp
