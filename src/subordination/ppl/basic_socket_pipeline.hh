#ifndef SUBORDINATION_PPL_BASIC_SOCKET_PIPELINE_HH
#define SUBORDINATION_PPL_BASIC_SOCKET_PIPELINE_HH

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/net/pstream>

#include <subordination/base/static_lock.hh>
#include <subordination/kernel/kstream.hh>
#include <subordination/ppl/basic_handler.hh>
#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/kernel_protocol.hh>

namespace sbn {

    class basic_socket_pipeline: public pipeline {

    public:
        typedef std::shared_ptr<basic_handler> event_handler_ptr;
        typedef std::unordered_map<sys::fd_type,event_handler_ptr> handler_container_type;
        typedef typename handler_container_type::const_iterator handler_const_iterator;
        typedef typename basic_handler::clock_type clock_type;
        typedef typename basic_handler::time_point time_point;
        typedef typename basic_handler::duration duration;

        using kernel_queue = std::queue<kernel*>;
        using mutex_type = std::recursive_mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = sys::event_poller;
        using thread_type = std::thread;

    private:
        // TODO probably we do not need this
        typedef static_lock<mutex_type, mutex_type> static_lock_type;

    private:
        mutex_type* _othermutex = nullptr;

    protected:
        kernel_queue _kernels;
        thread_type _thread;
        mutex_type _mutex;
        semaphore_type _semaphore;
        handler_container_type _handlers;
        kernel_protocol _protocol;
        duration _start_timeout = duration::zero();

    public:

        inline
        basic_socket_pipeline() {
            this->emplace_notify_handler(std::make_shared<basic_handler>());
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

        inline void foreign_pipeline(pipeline* rhs) noexcept {
            this->_protocol.foreign_pipeline(rhs);
        }

        inline void native_pipeline(pipeline* rhs) noexcept {
            this->_protocol.native_pipeline(rhs);
        }

        inline void remote_pipeline(pipeline* rhs) noexcept {
            this->_protocol.remote_pipeline(rhs);
        }

    protected:

        inline semaphore_type& poller() noexcept { return this->_semaphore; }
        inline const semaphore_type& poller() const noexcept { return this->_semaphore; }

        void
        emplace_handler(const sys::epoll_event& ev, const event_handler_ptr& ptr) {
            // N.B. we have two file descriptors (for the pipe)
            // in the process handler, so do not use emplace here
            this->log("add _, ev=_", *ptr, ev);
            this->_handlers[ev.fd()] = ptr;
            this->poller().insert(ev);
        }

        template <class X>
        void
        emplace_handler(const sys::epoll_event& ev, const std::shared_ptr<X>& ptr) {
            this->emplace_handler(ev, std::static_pointer_cast<basic_handler>(ptr));
        }

        void
        emplace_notify_handler(const event_handler_ptr& ptr) {
            sys::fd_type fd = this->poller().pipe_in();
            this->_handlers.emplace(fd, ptr);
        }

        template <class X>
        void
        emplace_notify_handler(const std::shared_ptr<X>& ptr) {
            this->emplace_notify_handler(
                std::static_pointer_cast<basic_handler>(ptr)
            );
        }

        inline void
        set_start_timeout(const duration& rhs) noexcept {
            this->_start_timeout = rhs;
        }

        virtual void process_kernels() = 0;
        virtual void loop();

    private:

        void
        flush_buffers(bool timeout) {
            const time_point now = timeout
                                   ? clock_type::now()
                                   : time_point(duration::zero());
            handler_const_iterator first = this->_handlers.begin();
            handler_const_iterator last = this->_handlers.end();
            while (first != last) {
                basic_handler& h = *first->second;
                if (h.stopped() || (timeout && this->is_timed_out(
                                            h,
                                            now
                                        ))) {
                    this->log("remove _ (_)", h, h.stopped() ? "stop" : "timeout");
                    h.remove(this->poller());
                    first = this->_handlers.erase(first);
                } else {
                    first->second->flush();
                    ++first;
                }
            }
        }

        handler_const_iterator
        handler_with_min_start_time_point() const noexcept {
            handler_const_iterator first = this->_handlers.begin();
            handler_const_iterator last = this->_handlers.end();
            handler_const_iterator result = last;
            while (first != last) {
                basic_handler& h = *first->second;
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
            if (result != last) {
                this->log("min _", *result->second);
            }
            return result;
        }

        void
        handle_events() {
            for (const sys::epoll_event& ev : this->poller()) {
                auto result = this->_handlers.find(ev.fd());
                if (result == this->_handlers.end()) {
                    this->log("unable to process fd _", ev.fd());
                } else {
                    basic_handler& h = *result->second;
                    // process event by calling event handler function
                    try {
                        h.handle(ev);
                    } catch (const std::exception& err) {
                        this->log("failed to process fd _: _", ev.fd(), err.what());
                    }
                    if (!ev) {
                        this->log("remove _ (bad event _)", h, ev);
                        h.remove(this->poller());
                        this->_handlers.erase(result);
                    }
                }
            }
        }

        bool
        is_timed_out(const basic_handler& rhs, const time_point& now) {
            return rhs.is_starting() &&
                   rhs.start_time_point() + this->_start_timeout <= now;
        }

    };

}

#endif // vim:filetype=cpp
