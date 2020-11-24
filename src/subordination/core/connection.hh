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
#include <unistdx/system/error>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/types.hh>

namespace sbn {

    enum class connection_flags: int {
        save_upstream_kernels = 1<<0,
        save_downstream_kernels = 1<<1,
        write_transaction_log = 1<<2
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
        using kernel_queue = std::deque<kernel_ptr>;
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
        id_type _counter = 1;
        sys::u32 _attempts = 1;
        const char* _name = "ppl";
        connection_state _state = connection_state::initial;

    protected:
        kernel_queue _upstream, _downstream;
        kernel_buffer _output_buffer;
        kernel_buffer _input_buffer;
        sys::socket_address _socket_address;

    public:
        connection() = default;
        virtual ~connection() = default;
        connection(const connection&) = delete;
        connection& operator=(const connection&) = delete;
        connection(connection&&) = delete;
        connection& operator=(connection&&) = delete;

        void send(kernel_ptr& k);

        inline void forward(foreign_kernel_ptr k) {
            do_forward(std::move(k));
        }

        void clear(kernel_sack& sack);

        /// \param[in] from socket address from which kernels are received
        void receive_kernels(const application* from_application=nullptr) {
            this->receive_kernels(from_application, [] (kernel_ptr&) {});
        }

        // TODO std::function is slow
        void receive_kernels(const application* from_application,
                             std::function<void(kernel_ptr&)> func);

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
        inline bool isset(connection_flags rhs) noexcept { return bool(this->_flags & rhs); }
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

        inline void min_input_buffer_size(size_t rhs) {
            if (this->_input_buffer.size() < rhs) { this->_input_buffer.resize(rhs); }
        }

        inline void min_output_buffer_size(size_t rhs) {
            if (this->_output_buffer.size() < rhs) { this->_output_buffer.resize(rhs); }
        }

    protected:

        foreign_kernel_ptr do_forward(foreign_kernel_ptr k);
        void recover_kernels(bool downstream);
        virtual void receive_foreign_kernel(foreign_kernel_ptr&& fk);

        struct flush_guard {
            kernel_buffer& _buffer;
            inline explicit flush_guard(kernel_buffer& buffer): _buffer(buffer) {
                this->_buffer.flip();
            }
            inline ~flush_guard() { this->_buffer.compact(); }
        };

        template <class Sink>
        inline void flush(Sink& sink) {
            flush_guard g(this->_output_buffer);
            this->_output_buffer.flush(sink);
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

        void write_kernel(const kernel* k) noexcept;
        kernel_ptr read_kernel(const application* from_application);
        void receive_kernel(kernel_ptr&& k, std::function<void(kernel_ptr&)> func);
        void plug_parent(kernel_ptr& k);
        kernel_ptr save_kernel(kernel_ptr k);
        void recover_kernel(kernel_ptr& k);

        template <class E> inline void
        log_write_error(const E& err) { this->log("write error _", err); }

        template <class E> inline void
        log_read_error(const E& err) { this->log("read error _", err); }

    protected:

        inline void generate_new_id(kernel* k) {
            k->id(++this->_counter);
        }

        inline void ensure_has_id(kernel* k) {
            if (!k->has_id()) { k->id(++this->_counter); }
        }

    };

    template <class Queue>
    inline typename Queue::const_iterator
    find_kernel(const sbn::kernel* a, const Queue& queue) {
        return std::find_if(queue.begin(), queue.end(),
                            [a] (const sbn::kernel_ptr& b) {
                                return a->id() == b->id() &&
                                    a->old_id() == b->old_id() &&
                                    a->source_application_id() == b->target_application_id() &&
                                    a->is_native() == b->is_native();
                            });
    }

}

#endif // vim:filetype=cpp
