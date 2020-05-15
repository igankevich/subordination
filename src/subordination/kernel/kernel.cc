#include <subordination/kernel/kernel.hh>

#include <subordination/base/error.hh>
#include <unistdx/base/make_object>

namespace {

    inline sbn::kernel::id_type
    get_id(const sbn::kernel* rhs) {
        return !rhs ? sbn::mobile_kernel::no_id() : rhs->id();
    }

}

void
sbn::kernel::read(sys::pstream& in) {
    base_kernel::read(in);
    bool b = false;
    in >> b;
    if (b) {
        this->setf(kernel_flag::carries_parent);
    }
    assert(not this->_parent);
    in >> this->_parent_id;
    assert(not this->_principal);
    in >> this->_principal_id;
    this->setf(kernel_flag::parent_is_id);
    this->setf(kernel_flag::principal_is_id);
}

void
sbn::kernel::write(sys::pstream& out) const {
    base_kernel::write(out);
    out << carries_parent();
    if (this->moves_downstream()) {
        out << this->_parent_id << this->_principal_id;
    } else {
        if (this->isset(kernel_flag::parent_is_id)) {
            out << this->_parent_id;
        } else {
            out << get_id(this->_parent);
        }
        if (this->isset(kernel_flag::principal_is_id)) {
            out << this->_principal_id;
        } else {
            out << get_id(this->_principal);
        }
    }
}

void
sbn::kernel::act() {}

void
sbn::kernel::react(kernel*) {
    SUBORDINATION_THROW(error, "empty react");
}

void
sbn::kernel::error(kernel* rhs) {
    this->react(rhs);
}

std::ostream&
sbn::operator<<(std::ostream& out, const kernel& rhs) {
    const char state[] = {
        (rhs.moves_upstream()   ? 'u' : '-'),
        (rhs.moves_downstream() ? 'd' : '-'),
        (rhs.moves_somewhere()  ? 's' : '-'),
        (rhs.moves_everywhere() ? 'b' : '-'),
        (rhs.carries_parent()   ? 'p' : '-'),
        0
    };
    return out << sys::make_object(
        "state", state,
        "type", typeid(rhs).name(),
        "id", rhs.id(),
        "src", rhs.from(),
        "dst", rhs.to(),
        "ret", rhs.return_code(),
        "app", rhs.app(),
        "parent", rhs._parent,
        "principal", rhs._principal
    );
}
