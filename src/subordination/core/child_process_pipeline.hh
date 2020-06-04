#ifndef SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH
#define SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH

#include <memory>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/process_handler.hh>

namespace sbn {

    class child_process_pipeline: public basic_socket_pipeline {

    private:
        using base_pipeline = basic_socket_pipeline;
        using event_handler_type = process_handler;
        using event_handler_ptr = std::shared_ptr<event_handler_type>;

    private:
        event_handler_ptr _parent;

    public:

        child_process_pipeline();
        virtual ~child_process_pipeline() = default;
        child_process_pipeline(child_process_pipeline&& rhs) = default;

        void send(kernel* k) override;

    protected:

        void process_kernels() override;

    private:

        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
