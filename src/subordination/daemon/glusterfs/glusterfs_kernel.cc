#include <fstream>

#include <subordination/bits/string.hh>
#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/glusterfs/glusterfs_kernel.hh>

void sbnd::glusterfs_state_kernel::write(sbn::kernel_buffer& out) const {
    out << this->_all_uuids;
}

void sbnd::glusterfs_state_kernel::read(sbn::kernel_buffer& in) {
    in >> this->_all_uuids;
}

void sbnd::glusterfs_kernel::act() {
    read_this_uuid();
}

void sbnd::glusterfs_kernel::react(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(glusterfs_kernel)) {
        auto ptr = sbn::pointer_dynamic_cast<glusterfs_state_kernel>(std::move(k));
        if (ptr->phase() == sbn::kernel::phases::downstream) {
        } else {
            add_to_all_uuids(ptr->all_uuids());
            ptr->uuid(this->_uuid);
            ptr->all_uuids(this->_all_uuids);
            ptr->return_to_parent();
            factory.remote().send(std::move(ptr));
        }
    }
}

void sbnd::glusterfs_kernel::read_this_uuid() {
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

void sbnd::glusterfs_kernel::uuid(uuid_type&& rhs) {
    using sbn::bits::trim_both;
    this->_all_uuids.erase(this->_uuid);
    this->_uuid = std::move(rhs);
    trim_both(this->_uuid);
    {
        auto g = factory.remote().guard();
        const auto& servers = factory.remote().servers();
        this->_all_uuids[this->_uuid] = servers.empty()
            ? sys::socket_address()
            : servers.front()->socket_address();
    }
    broadcast_uuids();
}

void sbnd::glusterfs_kernel::add_to_all_uuids(const uuid_map& other_uuids) {
    for (const auto& pair : other_uuids) {
        this->_all_uuids.insert(pair);
    }
}

void sbnd::glusterfs_kernel::broadcast_uuids() {
    auto k = sbn::make_pointer<glusterfs_state_kernel>();
    k->uuid(this->_uuid);
    k->all_uuids(this->_all_uuids);
    k->phase(sbn::kernel::phases::broadcast);
    k->principal_id(id());
    factory.remote().send(std::move(k));
}

sbnd::glusterfs_kernel::glusterfs_kernel(const Properties::GlusterFS& props):
_working_directory(props.working_directory) {}
