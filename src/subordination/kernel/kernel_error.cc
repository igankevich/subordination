#include "kernel_error.hh"

#include <ostream>

std::ostream&
bsc::operator<<(std::ostream& out, const kernel_error& rhs) {
    return out << rhs.what() << ": id=" << rhs.id();
}

