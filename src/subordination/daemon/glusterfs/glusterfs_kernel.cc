#include <fstream>
#include <sstream>

#include <unistdx/fs/idirectory>
#include <unistdx/net/hosts>

#include <subordination/bits/string.hh>
#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/glusterfs/glusterfs_kernel.hh>

namespace  {

    template <class ... Args> inline static void
    log(const char* fmt, const Args& ... args) {
        sys::log_message("glusterfs", fmt, args ...);
    }

}

void sbnd::glusterfs_kernel::act() {
    this->_fs->read_peer_uuids();
}

void sbnd::glusterfs_kernel::react(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(socket_pipeline_kernel)) {
        using e = socket_pipeline_event;
        auto child = sbn::pointer_dynamic_cast<socket_pipeline_kernel>(std::move(k));
        switch (child->event()) {
            case e::add_server:
            case e::remove_server: this->_fs->read_peer_uuids(); break;
            default: break;
        }
    }
}

void sbnd::gluster_file_system::read_this_uuid() {
    std::ifstream in(sys::path(this->_working_directory, "glusterd.info"));
    if (!in.is_open()) { return; }
    std::string line;
    while (std::getline(in, line, '\n')) {
        auto idx = line.find('=');
        if (idx == std::string::npos) { continue; }
        if (line.compare(0, idx, "UUID") != 0) { continue; }
        uuid(line.substr(idx+1));
        break;
    }
    in.close();
}

void sbnd::gluster_file_system::read_peer_uuids() {
    using sbn::bits::trim_both;
    this->_all_uuids.clear();
    std::vector<sys::interface_socket_address<sys::ipv4_address>> allowed_networks;
    {
        auto g = factory.remote().guard();
        const auto& servers = factory.remote().servers();
        for (const auto& s : servers) {
            allowed_networks.emplace_back(s->interface_socket_address());
        }
    }
    std::string line;
    sys::path peers_path(this->_working_directory, "peers");
    sys::idirectory dir;
    try {
        dir.open(peers_path);
        for (const auto& entry : dir) {
            if (sys::get_file_type(peers_path, entry) != sys::file_type::regular) { continue; }
            std::ifstream in(sys::path(peers_path, entry.name()));
            if (!in.is_open()) { continue; }
            uuid_type uuid;
            while (std::getline(in, line, '\n')) {
                auto idx = line.find('=');
                if (idx == std::string::npos) { continue; }
                if (line.compare(0, idx, "uuid") == 0) {
                    uuid = line.substr(idx+1);
                    trim_both(uuid);
                } else if (line.compare(0, std::min(size_t(8),idx), "hostname") == 0) {
                    auto hostname = line.substr(idx+1);
                    trim_both(hostname);
                    sys::hosts hosts;
                    hosts.family(sys::family_type::inet);
                    hosts.socket_type(sys::socket_type::stream);
                    hosts.update(hostname.data());
                    for (const auto& host : hosts) {
                        for (const auto& network : allowed_networks) {
                            if (network.contains(host.socket_address().addr4())) {
                                sys::socket_address a(host.socket_address(), network.port());
                                this->_all_uuids[uuid].emplace_back(a);
                            }
                        }
                    }
                }
            }
            in.close();
        }
    } catch (const std::exception& err) {
        log("failued to load peers from _", peers_path);
    }
    read_this_uuid();
}

void sbnd::gluster_file_system::uuid(uuid_type&& rhs) {
    using sbn::bits::trim_both;
    this->_all_uuids.erase(this->_uuid);
    this->_uuid = std::move(rhs);
    trim_both(this->_uuid);
    {
        auto g = factory.remote().guard();
        const auto& servers = factory.remote().servers();
        auto& addresses = this->_all_uuids[this->_uuid];
        addresses.clear();
        for (const auto& s : servers) {
            addresses.emplace_back(s->socket_address());
        }
    }
    for (const auto& pair : this->_all_uuids) {
        std::stringstream tmp;
        tmp << pair.first << ':';
        for (const auto& sa : pair.second) { tmp << ' ' << sa; }
        log(tmp.str().data());
    }
}

sbnd::glusterfs_kernel::glusterfs_kernel(const Properties::GlusterFS& props):
_fs(std::make_shared<gluster_file_system>(props)) {}

sbnd::gluster_file_system::gluster_file_system(const Properties::GlusterFS& props):
_working_directory(props.working_directory) {}

void sbnd::gluster_file_system::locate(const_path path,
                                       address_array& nodes) const noexcept {
    try {
        auto uuids = sys::const_path(path).attribute("trusted.glusterfs.list-node-uuids");
        auto first = uuids.data();
        std::string uuid;
        uuid.reserve(36);
        while (true) {
            if (*first == ' ' || !*first) {
                const auto& result = this->_all_uuids.find(uuid);
                if (result != this->_all_uuids.end()) {
                    for (const auto& addr : result->second) { nodes.emplace_back(addr); }
                }
                if (!*first) { break; }
                uuid.clear();
            } else {
                uuid += *first;
            }
            ++first;
        }
    } catch (const std::exception& err) {
        log("unable to determine location of _: _", path, err.what());
    }
}
