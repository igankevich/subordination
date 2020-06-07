#include <cstring>

#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_error.hh>
#include <subordination/core/kernel_type_registry.hh>

namespace  {

    inline void write_native(sbn::kernel_buffer* out, const sbn::kernel* k) {
        auto type = sbn::types.find(typeid(*k));
        if (type == sbn::types.end()) { throw std::invalid_argument("kernel type is null"); }
        out->write(type->id());
        k->write(*out);
    }

    inline sbn::kernel* read_native(sbn::kernel_buffer* in) {
        sbn::kernel_type::id_type id = 0;
        in->read(id);
        auto type = sbn::types.find(id);
        if (type == sbn::types.end()) { throw sbn::kernel_error("unknown kernel type", id); }
        auto k = type->construct();
        k->read(*in);
        return k;
    }

}

void sbn::kernel_buffer::write(foreign_kernel* k) { k->write(*this); }
void sbn::kernel_buffer::read(foreign_kernel* k) { k->read(*this); }

void sbn::kernel_buffer::write(kernel* k) {
    write_native(this, k);
    if (k->carries_parent()) {
        // embed parent into the packet
        auto* parent = k->parent();
        if (!parent) { throw std::invalid_argument("parent is null"); }
        write_native(this, parent);
    }
}

void sbn::kernel_buffer::read(kernel*& k) {
    k = read_native(this);
    if (k->carries_parent()) {
        auto* parent = read_native(this);
        k->parent(parent);
    }
}

void sbn::kernel_buffer::write(const sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = rhs.sockaddrlen();
    this->write(n);
    this->write(rhs.sockaddr(), rhs.sockaddrlen());
}

void sbn::kernel_buffer::read(sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = 0;
    this->read(n);
    this->read(rhs.sockaddr(), n);
}

void sbn::kernel_buffer::write(const sys::ipv4_address& rhs) {
    this->write(rhs.data(), rhs.size());
}

void sbn::kernel_buffer::read(sys::ipv4_address& rhs) {
    this->read(rhs.data(), rhs.size());
}

void sbn::kernel_buffer::write(const sys::ipv6_address& rhs) {
    this->write(rhs.data(), rhs.size());
}

void sbn::kernel_buffer::read(sys::ipv6_address& rhs) {
    this->read(rhs.data(), rhs.size());
}

template <class T>
void sbn::kernel_buffer::write(const sys::interface_address<T>& rhs) {
    this->write(rhs.address());
    this->write(rhs.netmask());
}

template void sbn::kernel_buffer::write(const sys::interface_address<sys::ipv4_address>&);
template void sbn::kernel_buffer::write(const sys::interface_address<sys::ipv6_address>&);

template <class T>
void sbn::kernel_buffer::read(sys::interface_address<T>& rhs) {
    T address{}, netmask{};
    this->read(address);
    this->read(netmask);
    rhs = sys::interface_address<T>(address, netmask);
}

template void sbn::kernel_buffer::read(sys::interface_address<sys::ipv4_address>&);
template void sbn::kernel_buffer::read(sys::interface_address<sys::ipv6_address>&);

sbn::kernel_write_guard::kernel_write_guard(kernel_frame& frame, kernel_buffer& buffer):
_frame(frame), _buffer(buffer), _old_position(buffer.position()) {
    this->_buffer.bump(sizeof(kernel_frame));
}

#include <unistdx/base/log_message>
sbn::kernel_write_guard::~kernel_write_guard() {
    auto new_position = this->_buffer.position();
    this->_frame.size(new_position - this->_old_position);
    this->_buffer.position(this->_old_position);
    if (this->_frame.size() > sizeof(kernel_frame)) {
        sys::log_message("WRITE", "frame size _", this->_frame.size());
        this->_buffer.write(&this->_frame, sizeof(kernel_frame));
        this->_buffer.position(new_position);
    }
}

sbn::kernel_read_guard::kernel_read_guard(kernel_frame& f, kernel_buffer& buffer):
frame(f), in(buffer), _old_limit(buffer.limit()) {
    if (in.remaining() < sizeof(kernel_frame)) { return; }
    std::memcpy(&frame, in.data()+in.position(), sizeof(kernel_frame));
    sys::log_message("READ", "frame size _", frame.size());
    if (in.remaining() >= frame.size()) {
        this->_good = true;
        in.limit(in.position() + frame.size());
        in.bump(sizeof(kernel_frame));
    }
}

sbn::kernel_read_guard::~kernel_read_guard() {
    in.position(in.limit());
    in.limit(this->_old_limit);
}
