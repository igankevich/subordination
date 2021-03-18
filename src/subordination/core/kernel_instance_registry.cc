#include <ostream>

#include <memory>
#include <vector>

#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/list.hh>

void sbn::kernel_instance_registry::clear(kernel_sack& sack) {
    for (const auto& pair : *this) { pair.second->mark_as_deleted(sack); }
}

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_instance_registry& rhs) {
    auto g = rhs.guard();
    return out << sbn::make_list_view(rhs);
}
