#ifndef SUBORDINATION_DAEMON_GLUSTERFS_GLUSTERFS_KERNEL_HH
#define SUBORDINATION_DAEMON_GLUSTERFS_GLUSTERFS_KERNEL_HH

#include <unordered_map>

#include <unistdx/fs/path>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/resident_kernel.hh>

namespace sbnd {

    using uuid_type = std::string;
    using uuid_map = std::unordered_map<uuid_type,sys::socket_address>;

    class glusterfs_state_kernel: public sbn::kernel {

    private:
        uuid_type _uuid;
        uuid_map _all_uuids;

    public:
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const uuid_type& uuid() const noexcept { return this->_uuid; }
        inline void uuid(const uuid_type& rhs) noexcept { this->_uuid = rhs; }
        inline const uuid_map& all_uuids() const noexcept { return this->_all_uuids; }
        inline void all_uuids(const uuid_map& rhs) noexcept { this->_all_uuids = rhs; }

    };

    class glusterfs_kernel: public sbn::kernel {

    private:
        /// Usually /var/lib/glusterd.
        sys::path _working_directory;
        /// UUID of the current node.
        uuid_type _uuid;
        /// UUID of all nodes including the current one.
        uuid_map _all_uuids;

    public:
        explicit glusterfs_kernel(const Properties::GlusterFS& props);
        void act() override;
        void react(sbn::kernel_ptr&& k) override;

    private:
        void read_this_uuid();
        void uuid(uuid_type&& rhs);
        void add_to_all_uuids(const uuid_map& other_uuids);
        void broadcast_uuids();

    };

}

#endif // vim:filetype=cpp
