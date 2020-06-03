#ifndef SUBORDINATION_PPL_BASIC_HANDLER_HH
#define SUBORDINATION_PPL_BASIC_HANDLER_HH

#include <chrono>
#include <iosfwd>

#include <unistdx/io/epoll_event>
#include <unistdx/io/poller>

#include <subordination/ppl/pipeline_base.hh>

namespace sbn {

    class basic_handler: public pipeline_base {

    public:
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using time_point = clock_type::time_point;

    private:
        time_point _start = time_point(duration::zero());

    public:

        virtual void
        handle(const sys::epoll_event& ev) {
            this->consume_pipe(ev.fd());
        }

        /// Called when the handler is removed from the poller.
        virtual void
        remove(sys::event_poller& poller) {}

        /// Flush dirty buffers (if needed).
        virtual void
        flush() {}

        virtual void
        write(std::ostream& out) const {
            out << "handler";
        }

        inline friend std::ostream&
        operator<<(std::ostream& out, const basic_handler& rhs) {
            rhs.write(out);
            return out;
        }

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

    private:

        void
        consume_pipe(sys::fd_type fd) {
            const size_t n = 20;
            char tmp[n];
            ssize_t c;
            while ((c = ::read(fd, tmp, n)) != -1) ;
        }


    };

}

#endif // vim:filetype=cpp
