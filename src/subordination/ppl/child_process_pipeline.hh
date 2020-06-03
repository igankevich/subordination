#ifndef SUBORDINATION_PPL_CHILD_PROCESS_PIPELINE_HH
#define SUBORDINATION_PPL_CHILD_PROCESS_PIPELINE_HH

#include <algorithm>
#include <iosfwd>
#include <memory>

#include <unistdx/io/pipe>

#include <subordination/ppl/basic_socket_pipeline.hh>
#include <subordination/ppl/process_handler.hh>

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
