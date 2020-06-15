#ifndef SUBORDINATION_DAEMON_JOB_STATUS_KERNEL_HH
#define SUBORDINATION_DAEMON_JOB_STATUS_KERNEL_HH

#include <subordination/core/application.hh>
#include <subordination/core/kernel.hh>

namespace sbnd {

    class Job_status_kernel: public sbn::kernel {

    public:
        using application_array = std::vector<sbn::application>;

    private:
        application_array _jobs;

    public:

        inline const application_array& jobs() const noexcept {
            return this->_jobs;
        }

        inline void jobs(const application_array& rhs) {
            this->_jobs = rhs;
        }

        inline void jobs(application_array&& rhs) {
            this->_jobs = std::move(rhs);
        }

        void read(sbn::kernel_buffer& in) override;
        void write(sbn::kernel_buffer& out) const override;

    };

}

#endif // vim:filetype=cpp
