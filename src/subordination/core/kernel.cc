#include <unistdx/base/make_object>

#include <subordination/core/error.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>

namespace {

    inline sbn::kernel::id_type
    get_id(const sbn::kernel* rhs) {
        return !rhs ? sbn::kernel::no_id() : rhs->id();
    }

}

sbn::kernel::~kernel() {
    if (bool(fields() & kernel_field::application)) {
        delete this->_application;
    }
}

void sbn::kernel::read(kernel_buffer& in) {
    in >> this->_result >> this->_id;
    bool b = false;
    in >> b;
    if (b) { this->setf(kernel_flag::carries_parent); }
    assert(not this->_parent);
    in >> this->_parent_id;
    assert(not this->_principal);
    in >> this->_principal_id;
    this->setf(kernel_flag::parent_is_id);
    this->setf(kernel_flag::principal_is_id);
}

void sbn::kernel::write(kernel_buffer& out) const {
    out << this->_result << this->_id;
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

void sbn::kernel::write_header(kernel_buffer& out) const {
    auto f = fields();
    if (source()) { f |= kernel_field::source; }
    if (destination()) { f |= kernel_field::destination; }
    out << f;
    if (bool(f & kernel_field::application)) { out << *application(); }
    else { out << application_id(); }
    if (bool(f & kernel_field::source)) { out << source(); }
    if (bool(f & kernel_field::destination)) { out << destination(); }
}

void sbn::kernel::read_header(kernel_buffer& in) {
    in >> this->_fields;
    if (bool(fields() & kernel_field::application)) {
        this->_application = new ::sbn::application;
        in >> *this->_application;
    } else {
        in >> this->_application_id;
    }
    if (bool(fields() & kernel_field::source)) { in >> this->_source; }
    if (bool(fields() & kernel_field::destination)) { in >> this->_destination; }
}

void sbn::kernel::swap_header(kernel* k) {
    std::swap(this->_fields, k->_fields);
    std::swap(this->_application, k->_application);
    std::swap(this->_source, k->_source);
    std::swap(this->_destination, k->_destination);
}

void
sbn::kernel::act() {}

void
sbn::kernel::react(kernel*) {
    throw ::sbn::error("empty react");
}

void
sbn::kernel::error(kernel* rhs) {
    this->react(rhs);
}

std::ostream& sbn::operator<<(std::ostream& out, const kernel& rhs) {
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
        "src", rhs.source(),
        "dst", rhs.destination(),
        "ret", rhs.return_code(),
        "app", rhs.application_id(),
        "parent", rhs._parent,
        "principal", rhs._principal
    );
}
