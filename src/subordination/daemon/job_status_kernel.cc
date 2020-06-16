#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/job_status_kernel.hh>

void sbnd::Job_status_kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out << this->_jobs << this->_job_ids;
}

void sbnd::Job_status_kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    in >> this->_jobs >> this->_job_ids;
}
