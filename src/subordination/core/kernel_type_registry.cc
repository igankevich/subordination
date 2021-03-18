#include <subordination/core/error.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/list.hh>

#include <algorithm>
#include <ostream>

sbn::kernel_type_registry::const_iterator
sbn::kernel_type_registry::find(id_type id) const noexcept {
    return std::find_if(this->begin(), this->end(),
                        [&id] (const kernel_type& rhs) { return rhs.id() == id; });
}

sbn::kernel_type_registry::const_iterator
sbn::kernel_type_registry::find(std::type_index idx) const noexcept {
    return std::find_if(this->begin(), this->end(),
                        [&idx] (const kernel_type& rhs) { return rhs.index() == idx; });
}

void
sbn::kernel_type_registry::add(kernel_type type) {
    const_iterator result;
    result = this->find(type.index());
    if (result != this->end()) {
        throw_error("kernel type index ", type.index().name(), " already exists");
    }
    if (!type.has_id()) { throw std::invalid_argument("bad id"); }
    result = this->find(type.id());
    if (result != this->end()) {
        throw_error("kernel type id ", type.id(), " already exists");
    }
    this->_types.emplace_back(type);
}

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_type_registry& rhs) {
    auto g = rhs.guard();
    return out << sbn::make_list_view(rhs);
}
