#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/terminate_kernel.hh>

void sbnd::Terminate_kernel::act() {
    {
        auto g = factory.process().guard();
        for (auto id : job_ids()) { factory.process().remove(id); }
    }
    factory.remote().send(this);
}

void sbnd::Terminate_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_job_ids;
}

void sbnd::Terminate_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_job_ids;
}
