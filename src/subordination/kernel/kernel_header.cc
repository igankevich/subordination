#include "kernel_header.hh"

#include <unistdx/base/make_object>
#include <ostream>

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_header& rhs) {
    out << sys::make_fields(
        "src", rhs._src,
        "dst", rhs._dst,
        "app", rhs._aid
    );
    out << ",aptr=";
    if (rhs.aptr()) {
        //out << *rhs.aptr();
        out << "<app>";
    } else {
        out << "<nullptr>";
    }
    return out;
}

void
sbn::kernel_header::write_header(sys::pstream& out) const {
    out << this->_flags;
    if (this->has_application()) {
        out << *this->_aptr;
    } else {
        out << this->_aid;
    }
    if (this->has_source_and_destination()) {
        out << this->_src << this->_dst;
    }
}

void
sbn::kernel_header::read_header(sys::pstream& in) {
    in >> this->_flags;
    if (this->has_application()) {
        this->_flags |= flag_type::owns_application;
        application* tmp = new application;
        in >> *tmp;
        this->_aptr = tmp;
        this->_aid = tmp->id();
    } else {
        // forcibly unset flag for security reasons
        this->_flags &= ~flag_type::owns_application;
        in >> this->_aid;
    }
    if (this->has_source_and_destination()) {
        in >> this->_src >> this->_dst;
    }
}

