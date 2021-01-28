#ifndef SUBORDINATION_CORE_BASIC_SOCKET_PIPELINE_HH
#define SUBORDINATION_CORE_BASIC_SOCKET_PIPELINE_HH

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/io/poller>

#include <subordination/bits/contracts.hh>
#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/connection_table.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/thread_pool.hh>
#include <subordination/core/transaction_log.hh>

namespace sbn {

    class basic_socket_pipeline: public pipeline {

    public:
        using connection_ptr = std::shared_ptr<connection>;
        using clock_type = typename connection::clock_type;
        using time_point = typename connection::time_point;
        using duration = typename connection::duration;
        using semaphore_type = sys::event_poller;

    protected:
        using mutex_type = std::recursive_mutex;
        using lock_type = std::unique_lock<mutex_type>;

    private:
        using kernel_queue = std::deque<kernel_ptr>;
        using size_type = connection_table::size_type;
        //using connection_table = std::unordered_map<sys::fd_type,connection_ptr>;
        using connection_const_iterator = typename connection_table::const_iterator;
        using kernel_array = std::vector<kernel*>;

    protected:
        kernel_queue _kernels;
        thread_pool _threads;
        mutable mutex_type _mutex;
        semaphore_type _semaphore;
        connection_table _connections;
        duration _connection_timeout = std::chrono::seconds(7);
        sys::u32 _max_connection_attempts = 1;
        pipeline* _foreign_pipeline = nullptr;
        pipeline* _native_pipeline = nullptr;
        pipeline* _remote_pipeline = nullptr;
        kernel_type_registry* _types = nullptr;
        kernel_instance_registry* _instances = nullptr;
        transaction_log* _transactions = nullptr;
        kernel_array _listeners;
        kernel_ptr_array _trash;
        size_t _min_input_buffer_size = 4096*16;
        size_t _min_output_buffer_size = 4096*16;

    public:
        class sentry {
        private:
            const basic_socket_pipeline& _pipeline;
        public:
            inline explicit sentry(const basic_socket_pipeline& rhs): _pipeline(rhs) { lock(); }
            inline ~sentry() { unlock(); }
            inline void lock() { this->_pipeline._mutex.lock(); }
            inline void unlock() { this->_pipeline._mutex.unlock(); }
        };

    public:
        class unsentry {
        private:
            const basic_socket_pipeline& _pipeline;
        public:
            inline explicit unsentry(const basic_socket_pipeline& rhs): _pipeline(rhs) {
                this->_pipeline._mutex.unlock();
            }
            inline ~unsentry() { this->_pipeline._mutex.lock(); }
        };

    public:

        inline sentry guard() noexcept { return sentry(*this); }
        inline sentry guard() const noexcept { return sentry(*this); }
        inline unsentry unguard() noexcept { return unsentry(*this); }
        inline unsentry unguard() const noexcept { return unsentry(*this); }

        basic_socket_pipeline();
        ~basic_socket_pipeline() = default;
        basic_socket_pipeline(basic_socket_pipeline&&) = delete;
        basic_socket_pipeline& operator=(basic_socket_pipeline&&) = delete;
        basic_socket_pipeline(const basic_socket_pipeline&) = delete;
        basic_socket_pipeline& operator=(const basic_socket_pipeline&) = delete;

        inline void send(kernel_ptr&& k) override {
            Expects(k);
            #if defined(SBN_DEBUG)
            this->log("send _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_kernels.emplace_back(std::move(k));
            this->_semaphore.notify_one();
        }

        inline void send(kernel_ptr_array&& kernels, size_t n) {
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) {
                Assert(kernels[i]);
                this->_kernels.emplace_back(std::move(kernels[i]));
            }
            this->_semaphore.notify_all();
        }

        void start();
        void stop();
        void wait();
        void clear(kernel_sack& sack);

        inline void write_transaction(transaction_status status, kernel_ptr& k) {
            Expects(k);
            if (auto* tr = transactions()) {
                k = tr->write({status, index(), std::move(k)});
            }
        }

        inline void foreign_pipeline(pipeline* rhs) noexcept { this->_foreign_pipeline = rhs; }
        inline void native_pipeline(pipeline* rhs) noexcept { this->_native_pipeline = rhs; }
        inline void remote_pipeline(pipeline* rhs) noexcept { this->_remote_pipeline = rhs; }
        inline void types(kernel_type_registry* rhs) noexcept { this->_types = rhs; }
        inline void instances(kernel_instance_registry* rhs) noexcept { this->_instances = rhs; }
        inline const pipeline* foreign_pipeline() const noexcept { return this->_foreign_pipeline; }
        inline pipeline* foreign_pipeline() noexcept { return this->_foreign_pipeline; }
        inline const pipeline* native_pipeline() const noexcept { return this->_native_pipeline; }
        inline pipeline* native_pipeline() noexcept { return this->_native_pipeline; }
        inline const pipeline* remote_pipeline() const noexcept { return this->_remote_pipeline; }
        inline pipeline* remote_pipeline() noexcept { return this->_remote_pipeline; }
        inline const kernel_type_registry* types() const noexcept { return this->_types; }
        inline kernel_type_registry* types() noexcept { return this->_types; }
        inline const kernel_instance_registry* instances() const noexcept { return this->_instances; }
        inline kernel_instance_registry* instances() noexcept { return this->_instances; }

        inline transaction_log* transactions() noexcept { return this->_transactions; }

        inline const transaction_log* transactions() const noexcept { return this->_transactions; }
        inline void min_input_buffer_size(size_t rhs) noexcept {
            this->_min_input_buffer_size = rhs;
        }

        inline void min_output_buffer_size(size_t rhs) noexcept {
            this->_min_output_buffer_size = rhs;
        }

        inline void transactions(transaction_log* rhs) noexcept { this->_transactions = rhs; }

        inline void trash(kernel_ptr&& k) {
            Expects(k);
            this->_trash.emplace_back(std::move(k));
        }

        void
        emplace_handler(const sys::epoll_event& ev, const connection_ptr& ptr) {
            Expects(ptr);
            // N.B. we have two file descriptors (for the pipe)
            // in the process connection, so do not use emplace here
            //this->log("add _", ptr->socket_address());
            this->_connections.emplace(ev.fd(), ptr);
            ptr->min_input_buffer_size(this->_min_input_buffer_size);
            ptr->min_output_buffer_size(this->_min_output_buffer_size);
            this->poller().insert(ev);
        }

        inline void erase(sys::fd_type fd) {
            Expects(fd);
            poller().erase(fd);
        }

        inline void erase_connection(sys::fd_type fd) {
            Expects(fd);
            poller().erase(fd);
            this->_connections.erase(fd);
        }

        inline const duration& connection_timeout() const noexcept {
            return this->_connection_timeout;
        }

        inline void connection_timeout(const duration& rhs) noexcept {
            this->_connection_timeout = rhs;
        }

        inline sys::u32 max_connection_attempts() const noexcept {
            return this->_max_connection_attempts;
        }

        inline void max_connection_attempts(sys::u32 rhs) noexcept {
            this->_max_connection_attempts = rhs;
        }

        inline const connection_table& connections() const noexcept {
            return this->_connections;
        }

        inline void add_listener(kernel* k) {
            Expects(k);
            this->_listeners.emplace_back(k);
        }

        void remove_listener(kernel* k);

        inline void cpus(const sys::cpu_set& cpus) noexcept {
            this->_threads.cpus(cpus);
        }

    protected:

        /// \brief This wrapper method prevents deadlock between socket and process pipelines.
        inline void send_to(pipeline* ppl, kernel_ptr&& k) {
            Expects(k);
            if (ppl) { auto g = unguard(); ppl->send(std::move(k)); }
        }

        inline void send_remote(kernel_ptr&& k) {
            Expects(k);
            send_to(remote_pipeline(), std::move(k));
        }

        inline void send_native(kernel_ptr&& k) {
            Expects(k);
            send_to(native_pipeline(), std::move(k));
        }

        inline void send_foreign(kernel_ptr&& k) {
            Expects(k);
            send_to(foreign_pipeline(), std::move(k));
        }

        /// \brief This wrapper method prevents deadlock between socket and process pipelines.
        inline void forward_to(pipeline* ppl, kernel_ptr&& k) {
            Expects(k);
            if (ppl) { auto g = unguard(); ppl->forward(std::move(k)); }
        }

        inline void forward_remote(kernel_ptr&& k) {
            Expects(k);
            forward_to(remote_pipeline(), std::move(k));
        }

        inline void forward_native(kernel_ptr&& k) {
            Expects(k);
            forward_to(native_pipeline(), std::move(k));
        }

        inline void forward_foreign(kernel_ptr&& k) {
            Expects(k);
            forward_to(foreign_pipeline(), std::move(k));
        }

        inline semaphore_type& poller() noexcept { return this->_semaphore; }
        inline const semaphore_type& poller() const noexcept { return this->_semaphore; }

        template <class X>
        void
        emplace_handler(const sys::epoll_event& ev, const std::shared_ptr<X>& ptr) {
            Expects(ptr);
            this->emplace_handler(ev, std::static_pointer_cast<connection>(ptr));
        }

        virtual void process_kernels() = 0;
        virtual void process_connections();
        virtual void loop();

    private:

        void flush_buffers();

        connection_const_iterator
        oldest_connection() const noexcept {
            auto first = this->_connections.begin(), last = this->_connections.end();
            auto result = last;
            while (first != last) {
                auto& conn = *first;
                //if (conn) {
                //    log("oldest _ _", conn->socket_address(), conn->state());
                //}
                if (conn &&
                    (conn->state() == connection::states::starting ||
                     conn->state() == connection::states::inactive) &&
                    conn->has_start_time_point()) {
                    if (result == last) {
                        result = first;
                    } else {
                        const auto old_t = (*result)->start_time_point();
                        const auto new_t = conn->start_time_point();
                        if (new_t < old_t) { result = first; }
                    }
                }
                ++first;
            }
            return result;
        }

        void handle_events();
        void deactivate(sys::fd_type fd, connection_ptr conn, const char* reason);
        void remove(sys::fd_type fd, connection_ptr& conn, const char* reason);

        friend class connection;
        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
