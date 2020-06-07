#include <ostream>

#include <unistdx/base/make_object>

#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_header.hh>

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_header& rhs) {
    out << sys::make_fields(
        "src", rhs._source,
        "dst", rhs._destination,
        "app", rhs._application_id
    );
    out << ",aptr=";
    if (rhs.application()) {
        //out << *rhs.aptr();
        out << "<app>";
    } else {
        out << "<nullptr>";
    }
    return out;
}

void sbn::kernel_header::write_header(kernel_buffer& out) const {
    auto f = fields();
    if (source()) { f |= kernel_field::source; }
    if (destination()) { f |= kernel_field::destination; }
    out << f;
    if (bool(f & kernel_field::application)) { out << *application(); }
    else { out << application_id(); }
    if (bool(f & kernel_field::source)) { out << source(); }
    if (bool(f & kernel_field::destination)) { out << destination(); }
}

void sbn::kernel_header::read_header(kernel_buffer& in) {
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
