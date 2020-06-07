#ifndef SUBORDINATION_CORE_CONNECTION_HH
#define SUBORDINATION_CORE_CONNECTION_HH

#include <chrono>
#include <deque>
#include <functional>

#include <unistdx/io/epoll_event>
#include <unistdx/io/poller>
#include <unistdx/net/socket_address>
#include <unistdx/util/system>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_proto_flag.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class connection: public pipeline_base {

    private:
        using kernel_queue = std::deque<kernel*>;
        using id_type = typename kernel::id_type;

    public:
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using time_point = clock_type::time_point;

    private:
        time_point _start{duration::zero()};
        basic_socket_pipeline* _parent = nullptr;
        kernel_proto_flag _flags{};
        kernel_queue _upstream, _downstream;
        id_type _counter = 0;

    protected:
        kernel_buffer _output_buffer{sys::page_size()};
        kernel_buffer _input_buffer{sys::page_size()};
        sys::socket_address _socket_address;

    public:
        connection() = default;
        ~connection() = default;
        connection(const connection&) = delete;
        connection& operator=(const connection&) = delete;
        connection(connection&&) = delete;
        connection& operator=(connection&&) = delete;

        void send(kernel* k);
        void forward(foreign_kernel* k);
        void clear();

        /// \param[in] from socket address from which kernels are received
        void receive_kernels(const application* from_application=nullptr) {
            this->receive_kernels(from_application, [] (kernel*) {});
        }

        void receive_kernels(const application* from_application,
                             std::function<void(kernel*)> func);

        virtual void handle(const sys::epoll_event& event);

        /// Called when the handler is removed from the poller.
        virtual void remove(sys::event_poller& poller);

        /// Flush dirty buffers (if needed).
        virtual void flush();

        inline time_point start_time_point() const noexcept { return this->_start; }

        inline bool
        has_start_time_point() const noexcept {
            return this->_start != time_point(duration::zero());
        }

        inline void
        setstate(pipeline_state rhs) noexcept {
            this->pipeline_base::setstate(rhs);
            if (rhs == pipeline_state::starting) { this->_start = clock_type::now(); }
        }

        inline basic_socket_pipeline* parent() const noexcept { return this->_parent; }
        inline void parent(basic_socket_pipeline* rhs) noexcept { this->_parent = rhs; }

        inline const sys::socket_address& socket_address() const noexcept {
            return this->_socket_address;
        }

        inline void socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline void setf(kernel_proto_flag rhs) noexcept { this->_flags |= rhs; }
        inline void unsetf(kernel_proto_flag rhs) noexcept { this->_flags &= ~rhs; }
        inline void flags(kernel_proto_flag rhs) noexcept { this->_flags = rhs; }
        inline kernel_proto_flag flags() const noexcept { return this->_flags; }

        inline void types(kernel_type_registry* rhs) noexcept {
            this->_output_buffer.types(rhs), this->_input_buffer.types(rhs);
        }

    protected:

        void recover_kernels(bool downstream);

        template <class Sink>
        inline void flush(Sink& sink) {
            this->_output_buffer.flip();
            this->_output_buffer.flush(sink);
            this->_output_buffer.compact();
        }

        template <class Source>
        inline void fill(Source& source) {
            this->_input_buffer.fill(source);
            this->_input_buffer.flip();
            log("remaining _", this->_input_buffer.remaining());
        }

    private:

        void write_kernel(kernel* k) noexcept;
        kernel* read_kernel(const application* from_application);
        bool receive_kernel(kernel* k);
        void plug_parent(kernel* k);
        bool save_kernel(kernel* k);
        void recover_kernel(kernel* k);

        template <class E> inline void
        log_write_error(const E& err) { this->log("write error _", err); }

        template <class E> inline void
        log_read_error(const E& err) { this->log("read error _", err); }

        inline void ensure_has_id(kernel* k) { if (!k->has_id()) { k->id(++this->_counter); } }

    };

}

#endif // vim:filetype=cpp
