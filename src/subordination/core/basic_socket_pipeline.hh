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

#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/static_lock.hh>

namespace sbn {

    class basic_socket_pipeline: public pipeline {

    public:
        using event_handler_ptr = std::shared_ptr<connection>;
        using event_handler_table = std::unordered_map<sys::fd_type,event_handler_ptr>;
        typedef typename event_handler_table::const_iterator handler_const_iterator;
        typedef typename connection::clock_type clock_type;
        typedef typename connection::time_point time_point;
        typedef typename connection::duration duration;

        using kernel_queue = std::queue<kernel*>;
        using mutex_type = std::recursive_mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = sys::event_poller;
        using thread_type = std::thread;

    private:
        // TODO probably we do not need this
        using static_lock_type = static_lock<mutex_type, mutex_type>;

    private:
        mutex_type* _othermutex = nullptr;

    protected:
        kernel_queue _kernels;
        thread_type _thread;
        mutex_type _mutex;
        semaphore_type _semaphore;
        event_handler_table _connections;
        duration _start_timeout = duration::zero();
        pipeline* _foreign_pipeline = nullptr;
        pipeline* _native_pipeline = nullptr;
        pipeline* _remote_pipeline = nullptr;
        kernel_type_registry* _types = nullptr;
        kernel_instance_registry* _instances = nullptr;

    public:

        inline
        basic_socket_pipeline() {
            this->emplace_notify_handler(std::make_shared<connection>());
        }

        ~basic_socket_pipeline() = default;
        basic_socket_pipeline(basic_socket_pipeline&&) = delete;
        basic_socket_pipeline& operator=(basic_socket_pipeline&&) = delete;
        basic_socket_pipeline(const basic_socket_pipeline&) = delete;
        basic_socket_pipeline& operator=(const basic_socket_pipeline&) = delete;

        inline void send(kernel* k) override {
            #if defined(SBN_DEBUG)
            this->log("send _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_kernels.emplace(k);
            this->_semaphore.notify_one();
        }

        inline void send(kernel** kernels, size_t n) {
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) { this->_kernels.emplace(kernels[i]); }
            this->_semaphore.notify_all();
        }

        void start();
        void stop();
        void wait();
        void clear();

        inline void
        set_other_mutex(mutex_type* rhs) noexcept {
            this->_othermutex = rhs;
        }

        inline mutex_type*
        other_mutex() noexcept {
            return this->_othermutex;
        }

        inline mutex_type*
        mutex() noexcept {
            return &this->_mutex;
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


    protected:

        inline semaphore_type& poller() noexcept { return this->_semaphore; }
        inline const semaphore_type& poller() const noexcept { return this->_semaphore; }

        void
        emplace_handler(const sys::epoll_event& ev, const event_handler_ptr& ptr) {
            // N.B. we have two file descriptors (for the pipe)
            // in the process handler, so do not use emplace here
            this->log("add _", ptr->socket_address());
            this->_connections[ev.fd()] = ptr;
            this->poller().insert(ev);
        }

        template <class X>
        void
        emplace_handler(const sys::epoll_event& ev, const std::shared_ptr<X>& ptr) {
            this->emplace_handler(ev, std::static_pointer_cast<connection>(ptr));
        }

        void
        emplace_notify_handler(const event_handler_ptr& ptr) {
            sys::fd_type fd = this->poller().pipe_in();
            this->_connections.emplace(fd, ptr);
        }

        template <class X>
        void
        emplace_notify_handler(const std::shared_ptr<X>& ptr) {
            this->emplace_notify_handler(
                std::static_pointer_cast<connection>(ptr)
            );
        }

        inline void
        set_start_timeout(const duration& rhs) noexcept {
            this->_start_timeout = rhs;
        }

        virtual void process_kernels() = 0;
        virtual void loop();

    private:

        void flush_buffers(bool timeout);

        handler_const_iterator
        handler_with_min_start_time_point() const noexcept {
            handler_const_iterator first = this->_connections.begin();
            handler_const_iterator last = this->_connections.end();
            handler_const_iterator result = last;
            while (first != last) {
                connection& h = *first->second;
                if (h.is_starting() && h.has_start_time_point()) {
                    if (result == last) {
                        result = first;
                    } else {
                        const auto old_t = result->second->start_time_point();
                        const auto new_t = h.start_time_point();
                        if (new_t < old_t) {
                            result = first;
                        }
                    }
                }
                ++first;
            }
            #if defined(SBN_DEBUG)
            if (result != last) {
                this->log("min _", result->second->socket_address());
            }
            #endif
            return result;
        }

        void
        handle_events() {
            for (const auto& ev : this->poller()) {
                auto result = this->_connections.find(ev.fd());
                if (result == this->_connections.end()) {
                    this->log("unable to process fd _", ev.fd());
                } else {
                    connection& h = *result->second;
                    // process event by calling event handler function
                    try {
                        h.handle(ev);
                    } catch (const std::exception& err) {
                        this->log("failed to process fd _: _", ev.fd(), err.what());
                    }
                    if (!ev) {
                        this->log("remove _ (bad event _)", h.socket_address(), ev);
                        h.remove(this->poller());
                        this->_connections.erase(result);
                    }
                }
            }
        }

        bool
        is_timed_out(const connection& rhs, const time_point& now) {
            return rhs.is_starting() &&
                   rhs.start_time_point() + this->_start_timeout <= now;
        }

    };

}

#endif // vim:filetype=cpp
