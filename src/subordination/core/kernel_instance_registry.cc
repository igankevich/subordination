#include <ostream>

#include <memory>
#include <vector>

#include <subordination/core/kernel_instance_registry.hh>

void sbn::kernel_instance_registry::clear(kernel_sack& sack) {
    for (const auto& pair : *this) { pair.second->mark_as_deleted(sack); }
}

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_instance_registry& rhs) {
    auto g = rhs.guard();
    for (const auto& pair : rhs) {
        const auto& kernel = pair.second;
        out << "/instance/" << kernel->id() << '=' << typeid(*kernel).name();
    }
    return out;
}
