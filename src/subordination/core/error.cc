#include <ostream>

#include <subordination/core/error.hh>

const char* sbn::error::what() const noexcept { return this->_what; }
void sbn::error::write(std::ostream& out) const { out << this->_what; }

std::ostream& sbn::operator<<(std::ostream& out, const error& rhs) {
    rhs.write(out); return out;
}

void sbn::type_error::write(std::ostream& out) const {
    out << what();
    const auto* args = this->_args.data();
    int i = 0;
    while (*args && i < 10) {
        args->write(out);
        ++args, ++i;
    }
}

void sbn::type_error::argument::write(std::ostream& out) const {
    switch (this->_type) {
        case type_char: out << this->_char; break;
        case type_short: out << this->_short; break;
        case type_int: out << this->_int; break;
        case type_long: out << this->_long; break;
        case type_long_long: out << this->_longlong; break;
        case type_unsigned_char: out << this->_char; break;
        case type_unsigned_short: out << this->_short; break;
        case type_unsigned_int: out << this->_int; break;
        case type_unsigned_long: out << this->_long; break;
        case type_unsigned_long_long: out << this->_longlong; break;
        case type_cstring: out << this->_cstring; break;
        default: break;
    }
}
