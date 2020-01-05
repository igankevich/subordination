#ifndef SUBORDINATION_PPL_BASIC_HANDLER_HH
#define SUBORDINATION_PPL_BASIC_HANDLER_HH

#include <iosfwd>

#include <unistdx/io/epoll_event>
#include <unistdx/io/poller>

#include <subordination/ppl/pipeline_base.hh>

namespace sbn {

    class basic_handler: public pipeline_base {

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
