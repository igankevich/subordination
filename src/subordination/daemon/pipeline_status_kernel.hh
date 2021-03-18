#ifndef SUBORDINATION_DAEMON_PIPELINE_STATUS_KERNEL_HH
#define SUBORDINATION_DAEMON_PIPELINE_STATUS_KERNEL_HH

#include <subordination/core/application.hh>
#include <subordination/core/kernel.hh>

namespace sbnd {

    class Pipeline_status_kernel: public sbn::service_kernel {

    public:
        struct Kernel {
            sbn::kernel::id_type id;
            sbn::kernel_type::id_type type_id;
            sbn::application::id_type source_application_id;
            sbn::application::id_type target_application_id;
            sys::socket_address source;
            sys::socket_address destination;
        };
        struct Connection {
            sys::socket_address address;
            std::vector<Kernel> kernels;
        };
        struct Pipeline {
            std::string name;
            std::vector<Connection> connections;
            std::vector<Kernel> kernels;
        };
        using pipeline_array = std::vector<Pipeline>;

    private:
        pipeline_array _pipelines;

    public:

        inline const pipeline_array& pipelines() const noexcept {
            return this->_pipelines;
        }

        inline void pipelines(const pipeline_array& rhs) {
            this->_pipelines = rhs;
        }

        inline void pipelines(pipeline_array&& rhs) {
            this->_pipelines = std::move(rhs);
        }

        void collect();

        void read(sbn::kernel_buffer& in) override;
        void write(sbn::kernel_buffer& out) const override;

    };

    sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Kernel& rhs);
    sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Connection& rhs);
    sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const sbnd::Pipeline_status_kernel::Pipeline& rhs);
    sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Kernel& rhs);
    sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Connection& rhs);
    sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, sbnd::Pipeline_status_kernel::Pipeline& rhs);

}

#endif // vim:filetype=cpp
