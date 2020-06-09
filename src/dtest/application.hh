#ifndef DTEST_APPLICATION_HH
#define DTEST_APPLICATION_HH

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/io/poller>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/process_group>
#include <unistdx/net/bridge_interface>
#include <unistdx/net/veth_interface>

#include <dtest/cluster.hh>
#include <dtest/cluster_node.hh>
#include <dtest/cluster_node_bitmap.hh>
#include <dtest/exit_code.hh>

namespace dts {

    class process_output {

    private:
        sys::byte_buffer _buffer;
        std::string _prefix;
        sys::fildes _in;
        sys::fd_type _out;

    public:

        inline explicit
        process_output(
            const std::string& prefix,
            sys::fildes&& in,
            sys::fd_type out,
            size_t size=4096
        ):
        _buffer{size}, _prefix(prefix), _in(std::move(in)), _out(out) {}

        void copy();

        inline const sys::fildes& in() const { return this->_in; }
        inline const sys::fd_type& out() const { return this->_out; }

    };

    class application {

    private:
        using duration = std::chrono::system_clock::duration;

    private:
        cluster _cluster;
        std::vector<sys::argstream> _arguments;
        std::vector<cluster_node_bitmap> _where;
        std::vector<cluster_node> _nodes;
        sys::process_group _procs;
        sys::bridge_interface _bridge;
        std::vector<sys::veth_interface> _veths;
        std::vector<process_output> _output;
        sys::event_poller _poller;
        std::thread _output_thread;
        exit_code _exitcode = exit_code::all;
        duration _execution_delay = duration::zero();
        char** _argv = nullptr;
        bool _restart = false;
        std::atomic<bool> _stopped{false};

    public:

        application() = default;
        inline explicit application(int argc, char* argv[]) { this->init(argc, argv); }

        void usage();
        void init(int argc, char* argv[]);
        void run();
        int wait();

        inline void send(sys::signal s) { log("received signal _", s); this->_procs.send(s); }
        inline void terminate() { this->send(sys::signal::terminate); }
        inline bool stopped() { return this->_stopped; }
        inline bool will_restart() const noexcept { return this->_restart; }

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("dtest", args...);
        }

    private:

        int accumulate_return_value();

    };

}

#endif // vim:filetype=cpp
