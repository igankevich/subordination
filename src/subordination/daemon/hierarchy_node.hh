#ifndef SUBORDINATION_DAEMON_HIERARCHY_NODE_HH
#define SUBORDINATION_DAEMON_HIERARCHY_NODE_HH

#include <iosfwd>

#include <unistdx/net/socket_address>

#include <subordination/core/resources.hh>
#include <subordination/core/types.hh>

namespace sbnd {

    class hierarchy_node {

    public:
        using resource_array = sbn::resource_array;

    private:
        resource_array _resources;

    public:

        hierarchy_node() = default;
        ~hierarchy_node() = default;
        hierarchy_node(const hierarchy_node&) = default;
        hierarchy_node& operator=(const hierarchy_node&) = default;
        hierarchy_node(hierarchy_node&&) = default;
        hierarchy_node& operator=(hierarchy_node&&) = default;

        inline explicit
        hierarchy_node(const resource_array& w) noexcept:
        _resources(w)
        {}

        inline void clear() noexcept { *this = {}; }

        inline bool operator==(const hierarchy_node& rhs) const noexcept {
            return this->_resources == rhs.resources();
        }

        inline bool operator!=(const hierarchy_node& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        /**
        \brief The sum of all the resources (e.g. thread concurrency)
        of each cluster node "behind" this one in the hierarchy.
        */
        inline const resource_array& resources() const noexcept { return this->_resources; }
        inline void resources(const resource_array& rhs) noexcept { this->_resources = rhs; }

        inline uint64_t num_threads() const noexcept {
            using r = sbn::resources::resources;
            return this->_resources[r::num_threads];
        }

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
