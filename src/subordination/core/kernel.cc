#include <subordination/core/error.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/list.hh>

namespace {

    inline sbn::kernel::id_type
    get_id(const sbn::kernel_ptr&& rhs) {
        return !rhs ? 0 : rhs->id();
    }

}

sbn::kernel::~kernel() {
    if (bool(this->_fields & fields::source_application)) {
        delete this->_source_application;
    }
    if (bool(this->_fields & fields::target_application)) {
        delete this->_target_application;
    }
}

void sbn::kernel::read(kernel_buffer& in) {
    in >> this->_result >> this->_id >> this->_old_id;
    in >> this->_at;
    in >> this->_flags;
    in >> this->_parent_id;
    in >> this->_principal_id;
    in >> this->_phase;
    in >> this->_path;
    in >> this->_weight;
    if (bool(this->_fields & fields::node_filter)) {
        this->_node_filter = sbn::resources::read(in);
    }
    this->_flags |= kernel_flag::parent_is_id;
    this->_flags |= kernel_flag::principal_is_id;
}

void sbn::kernel::write(kernel_buffer& out) const {
    out << this->_result << this->_id << this->_old_id;
    out << this->_at;
    out << this->_flags;
    out << parent_id();
    out << principal_id();
    out << this->_phase;
    out << this->_path;
    out << this->_weight;
    if (bool(this->_fields & fields::node_filter)) {
        this->_node_filter->write(out);
    }
}

void sbn::kernel::write_header(kernel_buffer& out) const {
    auto f = this->_fields;
    if (source()) { f |= fields::source; }
    if (destination()) { f |= fields::destination; }
    out << f;
    if (bool(f & fields::source_application)) { out << *source_application(); }
    else { out << source_application_id(); }
    if (bool(f & fields::target_application)) { out << *target_application(); }
    else { out << target_application_id(); }
    if (bool(f & fields::source)) { out << source(); }
    if (bool(f & fields::destination)) { out << destination(); }
}

void sbn::kernel::read_header(kernel_buffer& in) {
    in >> this->_fields;
    if (bool(this->_fields & fields::source_application)) {
        this->_source_application = new application;
        in >> *this->_source_application;
    } else {
        in >> this->_source_application_id;
    }
    if (bool(this->_fields & fields::target_application)) {
        this->_target_application = new application;
        in >> *this->_target_application;
    } else {
        in >> this->_target_application_id;
    }
    if (bool(this->_fields & fields::source)) { in >> this->_source; }
    if (bool(this->_fields & fields::destination)) { in >> this->_destination; }
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
    return out << list(
        list("state", state),
        list("type", typeid(rhs).name()),
        list("type-id", rhs.type_id()),
        list("id", rhs.id()),
        list("old-id", rhs.old_id()),
        list("src", make_string(rhs.source())),
        list("dst", make_string(rhs.destination())),
        list("ret", rhs.return_code()),
        list("src-app-id", rhs.source_application_id()),
        list("dst-app-id", rhs.target_application_id()),
        list("src-app", rhs.source_application()),
        list("dst-app", rhs.target_application()),
        list("parent", rhs._parent),
        list("principal", rhs._principal)
    );
}

std::ostream& sbn::operator<<(std::ostream& out, const kernel_ptr& rhs) {
    if (rhs) { out << *rhs.get(); }
    else { out << "<null-kernel>"; }
    return out;
}
