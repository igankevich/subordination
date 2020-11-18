#ifndef SUBORDINATION_DAEMON_HIERARCHY_NODE_HH
#define SUBORDINATION_DAEMON_HIERARCHY_NODE_HH

#include <iosfwd>

#include <unistdx/net/socket_address>

#include <subordination/core/types.hh>

namespace sbnd {

    class hierarchy_node {

    public:
        using weight_type = uint32_t;

    private:
        weight_type _weight = 1;

    public:

        hierarchy_node() = default;
        ~hierarchy_node() = default;
        hierarchy_node(const hierarchy_node&) = default;
        hierarchy_node& operator=(const hierarchy_node&) = default;
        hierarchy_node(hierarchy_node&&) = default;
        hierarchy_node& operator=(hierarchy_node&&) = default;

        inline explicit
        hierarchy_node(weight_type w) noexcept:
        _weight(w)
        {}

        inline void clear() noexcept { *this = {}; }

        inline bool operator==(const hierarchy_node& rhs) const noexcept {
            return this->_weight == rhs.weight();
        }

        inline bool operator!=(const hierarchy_node& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        /**
        \brief The sum of thread concurrency of each cluster node "behind"
        this one in the hierarchy.
        */
        inline weight_type weight() const noexcept { return this->_weight; }
        inline void weight(weight_type rhs) noexcept { this->_weight = rhs; }

        friend std::ostream&
        operator<<(std::ostream& out, const hierarchy_node& rhs);

        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

    };

    std::ostream&
    operator<<(std::ostream& out, const hierarchy_node& rhs);

    inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const hierarchy_node& rhs) {
        rhs.write(out); return out;
    }

    inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, hierarchy_node& rhs) {
        rhs.read(in); return in;
    }

}

#endif // vim:filetype=cpp
