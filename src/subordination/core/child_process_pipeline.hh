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
        size_t _pipe_buffer_size = std::numeric_limits<size_t>::max();

    public:

        child_process_pipeline() = default;
        virtual ~child_process_pipeline() = default;
        child_process_pipeline(child_process_pipeline&& rhs) = default;

        void send(kernel_ptr&& k) override;
        void add_connection();

        inline void pipe_buffer_size(size_t rhs) noexcept { this->_pipe_buffer_size = rhs; }

    protected:

        void process_kernels() override;

    private:

        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
