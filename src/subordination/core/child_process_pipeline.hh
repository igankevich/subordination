#ifndef SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH
#define SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH

#include <memory>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/process_handler.hh>

namespace sbn {

    class child_process_pipeline: public basic_socket_pipeline {

    private:
        using base_pipeline = basic_socket_pipeline;
        using connection_ptr = std::shared_ptr<process_handler>;

    private:
        connection_ptr _parent;

    public:

        child_process_pipeline() = default;
        virtual ~child_process_pipeline() = default;
        child_process_pipeline(child_process_pipeline&& rhs) = default;

        void send(kernel* k) override;
        void add_connection();

    protected:

        void process_kernels() override;

    private:

        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
