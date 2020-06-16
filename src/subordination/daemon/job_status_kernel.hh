#ifndef SUBORDINATION_DAEMON_JOB_STATUS_KERNEL_HH
#define SUBORDINATION_DAEMON_JOB_STATUS_KERNEL_HH

#include <subordination/core/application.hh>
#include <subordination/core/kernel.hh>

namespace sbnd {

    class Job_status_kernel: public sbn::kernel {

    public:
        using application_array = std::vector<sbn::application>;
        using application_id_array = std::vector<sbn::application::id_type>;

    private:
        application_array _jobs;
        application_id_array _job_ids;

    public:

        Job_status_kernel() = default;
        inline explicit Job_status_kernel(const application_id_array& ids): _job_ids(ids) {}

        inline const application_array& jobs() const noexcept {
            return this->_jobs;
        }

        inline void jobs(const application_array& rhs) {
            this->_jobs = rhs;
        }

        inline void jobs(application_array&& rhs) {
            this->_jobs = std::move(rhs);
        }

        inline const application_id_array& job_ids() const {
            return this->_job_ids;
        }

        inline void job_ids(const application_id_array& rhs) {
            this->_job_ids = rhs;
        }

        inline void job_ids(application_id_array&& rhs) {
            this->_job_ids = std::move(rhs);
        }

        void read(sbn::kernel_buffer& in) override;
        void write(sbn::kernel_buffer& out) const override;

    };

}

#endif // vim:filetype=cpp
