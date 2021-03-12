#ifndef SUBORDINATION_DAEMON_GLUSTERFS_GLUSTERFS_KERNEL_HH
#define SUBORDINATION_DAEMON_GLUSTERFS_GLUSTERFS_KERNEL_HH

#include <unordered_map>

#include <unistdx/base/log_message>
#include <unistdx/fs/path>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/file_system.hh>
#include <subordination/daemon/resident_kernel.hh>

namespace sbnd {

    using uuid_type = std::string;
    using uuid_map = std::unordered_map<uuid_type,std::vector<sys::socket_address>>;

    class gluster_file_system: public file_system {

    private:
        /// Usually /var/lib/glusterd.
        sys::path _working_directory{"/var/lib/glusterd"};
        /// UUID of the current node.
        uuid_type _uuid;
        /// UUID of all nodes including the current one.
        uuid_map _all_uuids;

    public:
        inline gluster_file_system(): file_system("glusterfs") {}
        explicit gluster_file_system(const Properties::GlusterFS& props);

        void locate(const_path path, address_array& nodes) const noexcept override;
        void read_this_uuid();
        void read_peer_uuids();
        void uuid(uuid_type&& rhs);

    };

    class glusterfs_kernel: public sbn::service_kernel {

    public:
        using gluster_file_system_ptr = std::shared_ptr<gluster_file_system>;

    private:
        gluster_file_system_ptr _fs;

    public:
        explicit glusterfs_kernel(const Properties::GlusterFS& props);
        void act() override;
        void react(sbn::kernel_ptr&& k) override;
        inline const gluster_file_system_ptr& file_system() const noexcept {
            return this->_fs;
        }
    };

}

#endif // vim:filetype=cpp
