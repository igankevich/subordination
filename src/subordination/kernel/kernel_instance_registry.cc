#include <subordination/kernel/kernel_instance_registry.hh>

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_instance_registry& rhs) {
    std::unique_lock<std::mutex> lock(rhs._mutex);
    for (const auto& pair : rhs._instances) {
        auto* kernel = pair.second;
        out << "/instance/" << kernel->id() << '=' << typeid(*kernel).name();
    }
    return out;
}

sbn::instance_registry_type sbn::instances;
