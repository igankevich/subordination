#include <unistdx/base/make_object>

#include <subordination/core/error.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>

namespace {

    inline sbn::kernel::id_type
    get_id(const sbn::kernel_ptr&& rhs) {
        return !rhs ? 0 : rhs->id();
    }

}

sbn::kernel::~kernel() {
    if (bool(fields() & kernel_field::source_application)) {
        delete this->_source_application;
    }
    if (bool(fields() & kernel_field::target_application)) {
        delete this->_target_application;
    }
}

void sbn::kernel::read(kernel_buffer& in) {
    in >> this->_result >> this->_id;
    in >> this->_at;
    in >> this->_flags;
    in >> this->_parent_id;
    in >> this->_principal_id;
    in >> this->_phase;
    this->_flags |= kernel_flag::parent_is_id;
    this->_flags |= kernel_flag::principal_is_id;
}

void sbn::kernel::write(kernel_buffer& out) const {
    out << this->_result << this->_id;
    out << this->_at;
    out << this->_flags;
    out << parent_id();
    out << principal_id();
    out << this->_phase;
}

void sbn::kernel::write_header(kernel_buffer& out) const {
    auto f = fields();
    if (source()) { f |= kernel_field::source; }
    if (destination()) { f |= kernel_field::destination; }
    out << f;
    if (bool(f & kernel_field::source_application)) { out << *source_application(); }
    else { out << source_application_id(); }
    if (bool(f & kernel_field::target_application)) { out << *target_application(); }
    else { out << target_application_id(); }
    if (bool(f & kernel_field::source)) { out << source(); }
    if (bool(f & kernel_field::destination)) { out << destination(); }
}

void sbn::kernel::read_header(kernel_buffer& in) {
    in >> this->_fields;
    if (bool(fields() & kernel_field::source_application)) {
        this->_source_application = new application;
        in >> *this->_source_application;
    } else {
        in >> this->_source_application_id;
    }
    if (bool(fields() & kernel_field::target_application)) {
        this->_target_application = new application;
        in >> *this->_target_application;
    } else {
        in >> this->_target_application_id;
    }
    if (bool(fields() & kernel_field::source)) { in >> this->_source; }
    if (bool(fields() & kernel_field::destination)) { in >> this->_destination; }
}

void sbn::kernel::swap_header(kernel* k) {
    std::swap(this->_fields, k->_fields);
    std::swap(this->_source_application, k->_source_application);
    std::swap(this->_target_application, k->_target_application);
    std::swap(this->_source, k->_source);
    std::swap(this->_destination, k->_destination);
}

void sbn::kernel::act() {}
void sbn::kernel::react(kernel_ptr&&) { throw ::sbn::error("empty react"); }
void sbn::kernel::rollback() {}

void sbn::kernel::mark_as_deleted(kernel_sack& result) {
    if (isset(kernel_flag::deleted)) { return; }
    setf(kernel_flag::deleted);
    result.emplace_back(this);
    if (is_native()) {
        if (auto* p = parent()) { p->mark_as_deleted(result); }
    }
}

std::ostream& sbn::operator<<(std::ostream& out, const kernel& rhs) {
    const char state[] = {
        (rhs.phase() == kernel::phases::upstream ? 'u' : '-'),
        (rhs.phase() == kernel::phases::downstream ? 'd' : '-'),
        (rhs.phase() == kernel::phases::point_to_point ? 'p' : '-'),
        (rhs.phase() == kernel::phases::broadcast ? 'b' : '-'),
        (rhs.carries_parent() ? 'c' : '-'),
        0
    };
    return out << sys::make_object(
        "state", state,
        "type", typeid(rhs).name(),
        "type-id", rhs.type_id(),
        "id", rhs.id(),
        "src", rhs.source(),
        "dst", rhs.destination(),
        "ret", rhs.return_code(),
        "src-app-id", rhs.source_application_id(),
        "dst-app-id", rhs.target_application_id(),
        "src-app", rhs.source_application(),
        "dst-app", rhs.target_application(),
        "parent", rhs._parent,
        "principal", rhs._principal
    );
}
