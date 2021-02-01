#include <cstring>

#include <subordination/bits/contracts.hh>
#include <subordination/core/error.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_type_registry.hh>

namespace  {

    /*
    template <class ... Args> inline void
    log(const Args& ... args) {
        sys::log_message("buffer", args...);
    }
    */

    inline void write_native(sbn::kernel_buffer* out, const sbn::kernel* k) {
        if (!out->types()) { sbn::throw_error("no kernel types"); }
        const auto& types = *out->types();
        auto g = types.guard();
        auto type = types.find(typeid(*k));
        if (type == types.end()) {
            sbn::throw_error("no kernel type for ", typeid(*k).name());
        }
        //auto old_position = out->position();
        out->write(type->id());
        k->write(*out);
        //auto new_position = out->position();
        //log("write-kernel size _ type-id _ type _",
        //    new_position-old_position, type->id(), typeid(*k).name());
    }

    inline sbn::kernel_ptr make_native(sbn::kernel_buffer* in) {
        sbn::kernel_type::id_type id = 0;
        in->read(id);
        if (!in->types()) { sbn::throw_error("no kernel types"); }
        const auto& types = *in->types();
        auto g = types.guard();
        auto type = types.find(id);
        if (type == types.end()) { sbn::throw_error("no kernel type for ", id); }
        static_assert(sizeof(sbn::kernel) <= sizeof(sbn::foreign_kernel), "bad size");
        auto k = type->construct();
        k->type_id(type->id());
        return k;
    }

    inline sbn::kernel_ptr read_native(sbn::kernel_buffer* in) {
        auto k = make_native(in);
        k->read(*in);
        return k;
    }

}

void sbn::kernel_buffer::write(const kernel* k) {
    Expects(k);
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

void sbn::kernel_buffer::read(kernel_ptr& k) {
    foreign_kernel_ptr fk(new foreign_kernel);
    fk->read_header(*this);
    if (fk->target_application_id() != this_application::id()) {
        fk->read(*this);
        k = std::move(fk);
    } else {
        k = make_native(this);
        k->swap_header(fk.get());
        fk.reset();
        k->read(*this);
        if (carry_all_parents()) {
            for (auto* p = k.get(); position() != limit(); p = p->parent()) {
                p->parent(read_native(this).release());
            }
        } else {
            if (k->carries_parent()) {
                k->parent(read_native(this).release());
            }
        }
    }
    Assert(k.get());
}

void sbn::kernel_buffer::write(const sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = rhs.size();
    this->write(n);
    this->write(rhs.get(), rhs.size());
}

void sbn::kernel_buffer::read(sys::socket_address& rhs) {
    // TODO we need more portable solution
    sys::u16 n = 0;
    this->read(n);
    this->read(rhs.get(), n);
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
        this->_buffer.write(&this->_frame, sizeof(kernel_frame));
        this->_buffer.position(new_position);
    }
}

sbn::kernel_read_guard::kernel_read_guard(kernel_frame& f, kernel_buffer& in):
_frame(f), _buffer(in), _old_limit(in.limit()) {
    if (in.remaining() < sizeof(kernel_frame)) { return; }
    std::memcpy(&this->_frame, in.data()+in.position(), sizeof(kernel_frame));
    if (in.remaining() >= this->_frame.size()) {
        this->_good = true;
        in.limit(in.position() + this->_frame.size());
        in.bump(sizeof(kernel_frame));
    }
}

sbn::kernel_read_guard::~kernel_read_guard() {
    if (good()) {
        this->_buffer.position(this->_buffer.limit());
        this->_buffer.limit(this->_old_limit);
    }
}
