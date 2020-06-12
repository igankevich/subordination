#include <cstring>

#include <unistdx/base/log_message>
#include <unistdx/util/backtrace>

#include <subordination/core/error.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_type_registry.hh>

namespace  {

    inline void write_native(sbn::kernel_buffer* out, const sbn::kernel* k) {
        if (!out->types()) { sbn::throw_error("no kernel types"); }
        const auto& types = *out->types();
        auto g = types.guard();
        auto type = types.find(typeid(*k));
        if (type == types.end()) {
            sbn::throw_error("no kernel type for ", typeid(*k).name());
        }
        out->write(type->id());
        k->write(*out);
    }

    inline sbn::kernel* read_native(sbn::kernel_buffer* in, sbn::foreign_kernel* ptr) {
        sbn::kernel_type::id_type id = 0;
        in->read(id);
        if (!in->types()) { sbn::throw_error("no kernel types"); }
        const auto& types = *in->types();
        auto g = types.guard();
        auto type = types.find(id);
        if (type == types.end()) { sbn::throw_error("no kernel type for ", id); }
        static_assert(sizeof(sbn::kernel) <= sizeof(sbn::foreign_kernel), "bad size");
        auto k = type->construct(ptr);
        k->read(*in);
        return k;
    }

}

void sbn::kernel_buffer::write(kernel* k) {
    k->write_header(*this);
    if (k->is_foreign()) {
        k->write(*this);
    } else {
        write_native(this, k);
        if (carry_all_parents()) {
            for (auto* p = k->parent(); p; p = p->parent()) {
                write_native(this, p);
            }
        } else {
            if (k->carries_parent()) {
                // embed parent into the packet
                auto* parent = k->parent();
                if (!parent) { throw std::invalid_argument("parent is null"); }
                write_native(this, parent);
            }
        }
    }
}

void sbn::kernel_buffer::read(kernel*& k) {
    std::unique_ptr<foreign_kernel> fk(new foreign_kernel);
    sys::log_message("TEST", "1 header _", *fk);
    fk->read_header(*this);
    sys::log_message("TEST", "header _", *fk);
    if (fk->target_application_id() != this_application::get_id()) {
        sys::log_message("TEST", "1");
        fk->read(*this);
        k = fk.release();
    } else {
        k = read_native(this, nullptr);
        k->swap_header(fk.get());
        fk.reset();
        if (carry_all_parents()) {
            for (auto* p = k; position() != limit(); p = p->parent()) {
                p->parent(read_native(this, nullptr));
            }
        } else {
            if (k->carries_parent()) {
                k->parent(read_native(this, nullptr));
            }
        }
    }
}

void sbn::kernel_buffer::write(const sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = rhs.sockaddrlen();
    sys::log_message("TEST", "write n _", n);
    this->write(n);
    this->write(rhs.sockaddr(), rhs.sockaddrlen());
}

void sbn::kernel_buffer::read(sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = 0;
    this->read(n);
    sys::log_message("TEST", "n _", n);
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

sbn::kernel_write_guard::~kernel_write_guard() {
    auto new_position = this->_buffer.position();
    this->_frame.size(new_position - this->_old_position);
    this->_buffer.position(this->_old_position);
    if (this->_frame.size() > sizeof(kernel_frame)) {
        sys::log_message("TEST", "write frame size _", this->_frame.size());
        this->_buffer.write(&this->_frame, sizeof(kernel_frame));
        this->_buffer.position(new_position);
    }
}

sbn::kernel_read_guard::kernel_read_guard(kernel_frame& f, kernel_buffer& buffer):
frame(f), in(buffer), _old_limit(buffer.limit()) {
    if (in.remaining() < sizeof(kernel_frame)) { return; }
    std::memcpy(&frame, in.data()+in.position(), sizeof(kernel_frame));
    if (in.remaining() >= frame.size()) {
        sys::log_message("TEST", "read frame size _", frame.size());
        this->_good = true;
        in.limit(in.position() + frame.size());
        in.bump(sizeof(kernel_frame));
    }
}

sbn::kernel_read_guard::~kernel_read_guard() {
    in.position(in.limit());
    in.limit(this->_old_limit);
}
