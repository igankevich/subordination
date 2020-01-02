#include "kernel_type_error.hh"

std::ostream&
bsc::operator<<(std::ostream& out, const kenel_type_error& rhs) {
    operator<<(out, static_cast<const error&>(rhs));
    return out << rhs.what() << ": '" << rhs._tp1 << "' and '"
        << rhs._tp2 << "'";
}

