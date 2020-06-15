#ifndef SUBORDINATION_CORE_CONNECTION_HH
#define SUBORDINATION_CORE_CONNECTION_HH

#include <chrono>
#include <deque>
#include <functional>
#include <memory>

#include <unistdx/base/flag>
#include <unistdx/base/log_message>
#include <unistdx/io/epoll_event>
#include <unistdx/io/poller>
#include <unistdx/net/socket_address>
#include <unistdx/util/system>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/types.hh>

namespace sbn {

    enum class connection_flags: int {
        save_upstream_kernels = 1<<0,
        save_downstream_kernels = 1<<1,
        write_transaction_log = 1<<2,
    };

    UNISTDX_FLAGS(connection_flags)

    enum class connection_state {
        initial,
        starting,
        started,
        stopping,
        stopped,
        inactive,
    };

    const char* to_string(connection_state rhs);
    std::ostream& operator<<(std::ostream& out, connection_state rhs);

    class connection {

    private:
        using kernel_queue = std::deque<kernel*>;
        using id_type = typename kernel::id_type;

    public:
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using time_point = clock_type::time_point;
        using connection_ptr = std::shared_ptr<connection>;

    private:
        time_point _start{duration::zero()};
        basic_socket_pipeline* _parent = nullptr;
        connection_flags _flags{};
        kernel_queue _upstream, _downstream;
        id_type _counter = 0;
        sys::u32 _attempts = 1;
        const char* _name = "ppl";
        connection_state _state = connection_state::initial;

    protected:
        kernel_buffer _output_buffer{sys::page_size()};
        kernel_buffer _input_buffer{sys::page_size()};
        sys::socket_address _socket_address;

    public:
        connection() = default;
        virtual ~connection() = default;
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

        // TODO std::function is slow
        void receive_kernels(const application* from_application,
                             std::function<void(kernel*)> func);

        virtual void handle(const sys::epoll_event& event);

        virtual void add(const connection_ptr& self);
        virtual void remove(const connection_ptr& self);
        virtual void retry(const connection_ptr& self);
        virtual void deactivate(const connection_ptr& self);
        virtual void activate(const connection_ptr& self);
        virtual void flush();
        virtual void stop();

        inline time_point start_time_point() const noexcept { return this->_start; }
        inline void start_time_point(time_point rhs) noexcept { this->_start = rhs; }

        inline bool
        has_start_time_point() const noexcept {
            return this->_start != time_point(duration::zero());
        }

        inline basic_socket_pipeline* parent() const noexcept { return this->_parent; }
        inline void parent(basic_socket_pipeline* rhs) noexcept { this->_parent = rhs; }

        inline const sys::socket_address& socket_address() const noexcept {
            return this->_socket_address;
        }

        inline void socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline void setf(connection_flags rhs) noexcept { this->_flags |= rhs; }
        inline void unsetf(connection_flags rhs) noexcept { this->_flags &= ~rhs; }
        inline void flags(connection_flags rhs) noexcept { this->_flags = rhs; }
        inline connection_flags flags() const noexcept { return this->_flags; }

        inline void types(kernel_type_registry* rhs) noexcept {
            this->_output_buffer.types(rhs), this->_input_buffer.types(rhs);
        }

        inline sys::u32 attempts() const noexcept { return this->_attempts; }
        inline const char* name() const noexcept { return this->_name; }
        inline void name(const char* rhs) noexcept { this->_name = rhs; }
        inline connection_state state() const noexcept { return this->_state; }
        inline void state(connection_state rhs) noexcept {
            this->_state = rhs;
            if (rhs == connection_state::starting) { this->_start = clock_type::now(); }
        }

        inline const kernel_queue& upstream() const noexcept { return this->_upstream; }
        inline const kernel_queue& downstream() const noexcept { return this->_downstream; }

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
        }

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message(this->_name, args ...);
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
