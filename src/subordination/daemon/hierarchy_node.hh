#ifndef SUBORDINATION_DAEMON_HIERARCHY_NODE_HH
#define SUBORDINATION_DAEMON_HIERARCHY_NODE_HH

#include <chrono>
#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/net/socket_address>

#include <subordination/core/resources.hh>
#include <subordination/core/types.hh>

namespace sbnd {

    class hierarchy_node {

    public:
        using resource_array = sbn::resource_array;
        using socket_address_array = std::vector<sys::socket_address>;
        using container_type = std::unordered_map<sys::socket_address,hierarchy_node>;
        using version_type = uint64_t;
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;

    private:
        sys::socket_address _socket_address;
        sys::socket_address _superior_socket_address;
        resource_array _resources;
        version_type _version{0};
        time_point _last_modified;

    public:

        hierarchy_node() = default;
        ~hierarchy_node() = default;
        hierarchy_node(const hierarchy_node&) = default;
        hierarchy_node& operator=(const hierarchy_node&) = default;
        hierarchy_node(hierarchy_node&&) = default;
        hierarchy_node& operator=(hierarchy_node&&) = default;

        inline explicit
        hierarchy_node(const sys::socket_address& a, const resource_array& w) noexcept:
        _socket_address(a), _resources(w) {}

        inline explicit
        hierarchy_node(const resource_array& w) noexcept: _resources(w) {}

        inline void clear() {
            this->_socket_address.clear();
            this->_superior_socket_address.clear();
            this->_resources.clear();
        }

        inline bool operator==(const hierarchy_node& rhs) const noexcept {
            return this->_socket_address == rhs._socket_address &&
                this->_superior_socket_address == rhs._superior_socket_address &&
                this->_resources == rhs.resources();
        }

        inline bool operator!=(const hierarchy_node& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        inline bool has_superior() const noexcept {
            return bool(this->_superior_socket_address);
        }

        inline const sys::socket_address& socket_address() const noexcept {
            return this->_socket_address;
        }

        inline void socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline const sys::socket_address& superior_socket_address() const noexcept {
            return this->_superior_socket_address;
        }

        inline void superior_socket_address(const sys::socket_address& rhs) noexcept {
            this->_superior_socket_address = rhs;
        }

        /**
        \brief The sum of all the resources (e.g. thread concurrency)
        of each cluster node "behind" this one in the hierarchy.
        */
        inline const resource_array& resources() const noexcept { return this->_resources; }
        inline void resources(const resource_array& rhs) noexcept { this->_resources = rhs; }

        inline version_type version() const noexcept { return this->_version; }
        inline void version(version_type rhs) noexcept { this->_version = rhs; }

        inline void increment_version() noexcept {
            if (this->_version < std::numeric_limits<version_type>::max()) {
                ++this->_version;
            }
        }

        inline time_point last_modified() const noexcept { return this->_last_modified; }
        inline void last_modified(time_point rhs) noexcept { this->_last_modified = rhs; }

        inline uint64_t total_threads() const noexcept {
            using r = sbn::resources::resources;
            return this->_resources[r::total_threads];
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
