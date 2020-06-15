#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/pipeline_status_kernel.hh>

sbn::kernel_buffer& sbnd::operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Kernel& rhs) {
    return out << rhs.id << rhs.type_id
        << rhs.source_application_id << rhs.target_application_id
        << rhs.source << rhs.destination;
}

sbn::kernel_buffer& sbnd::operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Connection& rhs) {
    return out << rhs.address << rhs.kernels;
}

sbn::kernel_buffer& sbnd::operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Pipeline& rhs) {
    return out << rhs.name << rhs.connections;
}

sbn::kernel_buffer& sbnd::operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Kernel& rhs) {
    return in >> rhs.id >> rhs.type_id
        >> rhs.source_application_id >> rhs.target_application_id
        >> rhs.source >> rhs.destination;
}

sbn::kernel_buffer& sbnd::operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Connection& rhs) {
    return in >> rhs.address >> rhs.kernels;
}

sbn::kernel_buffer& sbnd::operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Pipeline& rhs) {
    return in >> rhs.name >> rhs.connections;
}

void sbnd::Pipeline_status_kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out << this->_pipelines;
}

void sbnd::Pipeline_status_kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    in >> this->_pipelines;
}
