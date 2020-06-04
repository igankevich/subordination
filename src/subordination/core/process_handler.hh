#ifndef SUBORDINATION_CORE_PROCESS_HANDLER_HH
#define SUBORDINATION_CORE_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

#include <unistdx/io/fildes_pair>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/ipc/process>

#include <subordination/core/application.hh>
#include <subordination/core/basic_handler.hh>
#include <subordination/core/kernel_protocol.hh>
#include <subordination/core/kstream.hh>
#include <subordination/core/pipeline_base.hh>

namespace sbn {

    class process_handler: public basic_handler {

    private:
        typedef sys::basic_fildesbuf<char, std::char_traits<char>, sys::fildes_pair>
            fildesbuf_type;
        typedef basic_kernelbuf<fildesbuf_type> kernelbuf_type;
        typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
        typedef kstream stream_type;

        enum class role_type {child, parent};

    private:
        sys::pid_type _childpid;
        kernelbuf_ptr _buffer;
        stream_type _stream;
        kernel_protocol* _protocol = nullptr;
        application _application;
        role_type _role;

    public:

        /// Called from parent process.
        process_handler(sys::pid_type&& child,
                        sys::two_way_pipe&& pipe,
                        const application& app);

        /// Called from child process.
        explicit process_handler(sys::pipe&& pipe);

        virtual
        ~process_handler() {
            // recover kernels from upstream and downstream buffer
            this->_protocol->recover_kernels(true);
        }

        const sys::pid_type&
        childpid() const {
            return this->_childpid;
        }

        const application&
        app() const noexcept {
            return this->_application;
        }

        void
        close() {
            this->_buffer->fd().close();
        }

        void
        send(kernel* k) {
            this->_protocol->send(k, this->_stream, sys::socket_address{});
        }

        void
        handle(const sys::epoll_event& event) override;

        void
        flush() override {
            if (this->_buffer->dirty()) {
                this->_buffer->pubflush();
            }
        }

        void
        write(std::ostream& out) const override;

        void
        remove(sys::event_poller& poller) override;

        void
        forward(foreign_kernel* k) {
            // remove application before forwarding
            // to child process
            k->aptr(nullptr);
            this->_protocol->forward(k, this->_stream);
        }

        inline void
        name(const char* rhs) noexcept {
            this->pipeline_base::name(rhs);
            this->_protocol->name(rhs);
            #if defined(SBN_DEBUG)
            if (this->_buffer) {
                this->_buffer->set_name(rhs);
            }
            #endif
        }

        inline sys::fd_type
        in() const noexcept {
            return this->_buffer->fd().in().fd();
        }

        inline sys::fd_type
        out() const noexcept {
            return this->_buffer->fd().out().fd();
        }

        inline void protocol(kernel_protocol* rhs) noexcept { this->_protocol = rhs; }

    };

}

#endif // vim:filetype=cpp
