#ifndef SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH
#define SUBORDINATION_CORE_CHILD_PROCESS_PIPELINE_HH

#include <memory>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/process_handler.hh>

namespace sbn {

    class child_process_pipeline: public basic_socket_pipeline {

    public:
        struct properties: public sbn::basic_socket_pipeline::properties {
            size_t pipe_buffer_size;

            inline properties():
            properties{sys::this_process::cpu_affinity(), sys::page_size()} {}

            inline explicit
            properties(const sys::cpu_set& cpus, size_t page_size, size_t multiple=16):
            sbn::basic_socket_pipeline::properties{cpus, page_size, multiple},
            pipe_buffer_size{page_size*multiple} {}

            bool set(const char* key, const std::string& value);
        };

    private:
        using base_pipeline = basic_socket_pipeline;
        using connection_ptr = std::shared_ptr<process_handler>;

    private:
        connection_ptr _parent;
        size_t _pipe_buffer_size = 4096UL*16UL;

    public:

        explicit child_process_pipeline(const properties& p);
        child_process_pipeline() = default;
        virtual ~child_process_pipeline() = default;
        child_process_pipeline(const child_process_pipeline&) = delete;
        child_process_pipeline& operator=(const child_process_pipeline&) = delete;
        child_process_pipeline(child_process_pipeline&&) = delete;
        child_process_pipeline& operator=(child_process_pipeline&&) = delete;

        void send(kernel_ptr&& k) override;
        void add_connection();

        inline void pipe_buffer_size(size_t rhs) noexcept { this->_pipe_buffer_size = rhs; }
        void write(std::ostream& out) const override;

    protected:

        void process_kernels() override;

    private:

        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
