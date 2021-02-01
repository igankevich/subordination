#ifndef SUBORDINATION_DAEMON_TERMINATE_KERNEL_HH
#define SUBORDINATION_DAEMON_TERMINATE_KERNEL_HH

#include <subordination/core/kernel.hh>

namespace sbnd {

    class Terminate_kernel: public sbn::service_kernel {

    public:
        using application_id_array = std::vector<sbn::application::id_type>;

    private:
        application_id_array _job_ids;

    public:
        inline explicit Terminate_kernel(const application_id_array& ids): _job_ids(ids) {}
        Terminate_kernel() = default;

        inline const application_id_array& job_ids() const { return this->_job_ids; }
        inline void job_ids(const application_id_array& rhs) { this->_job_ids = rhs; }
        inline void job_ids(application_id_array&& rhs) { this->_job_ids = std::move(rhs); }

        void act() override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    };

}

#endif // vim:filetype=cpp
