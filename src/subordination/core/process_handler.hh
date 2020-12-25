#ifndef SUBORDINATION_CORE_PROCESS_HANDLER_HH
#define SUBORDINATION_CORE_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

#include <unistdx/io/fildes_pair>
#include <unistdx/io/poller>
#include <unistdx/ipc/process>

#include <subordination/core/application.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/weights.hh>

namespace sbn {

    class process_handler: public connection {

    private:
        enum class roles {child, parent};

    private:
        sys::pid_type _child_process_id;
        sys::fildes_pair _file_descriptors;
        ::sbn::application _application;
        roles _role;
        sbn::weight_array _load{};
        int _num_active_kernels = 0;
        int _kernel_count_last = 0;
        time_point _last{};
        foreign_kernel_ptr _main_kernel{};
        pipeline* _unix{};

    public:

        /// Called from parent process.
        process_handler(sys::pid_type child,
                        sys::two_way_pipe&& pipe,
                        const ::sbn::application& app);

        /// Called from child process.
        explicit process_handler(sys::pipe&& pipe);

        virtual
        ~process_handler() {
            // recover kernels from upstream and downstream buffer
            recover_kernels(true);
        }

        void handle(const sys::epoll_event& event) override;
        void add(const connection_ptr& self) override;
        void remove(const connection_ptr& self) override;
        void flush() override;
        void stop() override;

        void forward(foreign_kernel_ptr&& k);

        inline const ::sbn::application& application() const noexcept {
            return this->_application;
        }

        inline sys::pid_type child_process_id() const noexcept {
            return this->_child_process_id;
        }

        inline bool stale(time_point now, duration timeout) noexcept {
            if (now-this->_last < timeout) { return false; }
            bool changed = this->_num_active_kernels != this->_kernel_count_last;
            this->_kernel_count_last = this->_num_active_kernels;
            this->_last = now;
            return !changed;
        }

        inline sys::fd_type in() const noexcept { return this->_file_descriptors.in().fd(); }
        inline sys::fd_type out() const noexcept { return this->_file_descriptors.out().fd(); }
        inline pipeline* unix() const noexcept { return this->_unix; }
        inline void unix(pipeline* rhs) noexcept { this->_unix = rhs; }

        /// The number of kernels that were sent to the process, but have not returned yet.
        inline int num_active_kernels() const noexcept { return this->_num_active_kernels; }

    protected:
        void receive_kernel(kernel_ptr&& k) override;
        void receive_foreign_kernel(foreign_kernel_ptr&& fk) override;
        kernel_ptr read_kernel() override;
        void write_kernel(const kernel* k) noexcept override;

    };

}

#endif // vim:filetype=cpp
