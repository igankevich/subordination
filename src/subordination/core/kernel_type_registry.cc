#include <subordination/core/kernel_type_registry.hh>

#include <algorithm>
#include <ostream>

#include <subordination/core/kernel_error.hh>

sbn::kernel_type_registry::const_iterator
sbn::kernel_type_registry::find(id_type id) const noexcept {
    return std::find_if(
        this->begin(), this->end(),
        [&id] (const kernel_type& rhs) {
            return rhs.id() == id;
        }
    );
}

sbn::kernel_type_registry::const_iterator
sbn::kernel_type_registry::find(std::type_index idx) const noexcept {
    return std::find_if(
        this->begin(), this->end(),
        [&idx] (const kernel_type& rhs) {
            return rhs.index() == idx;
        }
    );
}

void
sbn::kernel_type_registry::register_type(kernel_type type) {
    const_iterator result;
    result = this->find(type.index());
    if (result != this->end()) {
        SUBORDINATION_THROW(kenel_type_error, type, *result);
    }
    if (!type.has_id()) { throw std::invalid_argument("bad id"); }
    result = this->find(type.id());
    if (result != this->end()) {
        SUBORDINATION_THROW(kenel_type_error, type, *result);
    }
    this->_types.emplace_back(type);
}

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_type_registry& rhs) {
    for (const auto& type : rhs._types) {
        out << "/type/" << type.id() << '/' << type;
    }
    return out;
}

sbn::kernel*
sbn::kernel_type_registry::read_object(sys::pstream& packet) {
    id_type id;
    packet >> id;
    const_iterator result = this->find(id);
    if (result == this->end()) {
        throw kernel_error("unknown kernel type", id);
    }
    return result->read(packet);
}

sbn::kernel_type_registry sbn::types;
