#ifndef DTEST_APPLICATION_HH
#define DTEST_APPLICATION_HH

#include <atomic>
#include <chrono>
#include <functional>
#include <queue>
#include <thread>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/io/poller>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/process_group>

#include <dtest/cluster.hh>
#include <dtest/cluster_node.hh>
#include <dtest/cluster_node_bitmap.hh>
#include <dtest/exit_code.hh>

namespace dts {

    class process_output {

    public:
        using line_array = std::vector<std::string>;

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

        void copy(line_array& lines);

        inline const sys::fildes& in() const { return this->_in; }
        inline const sys::fd_type& out() const { return this->_out; }

    };

    class application {

    public:
        using line_array = process_output::line_array;
        using test_function = std::function<bool(application&, const line_array&)>;
        using test_queue = std::queue<test_function>;

    private:
        using duration = std::chrono::system_clock::duration;

    private:
        cluster _cluster;
        std::vector<sys::argstream> _arguments;
        std::vector<cluster_node_bitmap> _where;
        sys::process_group _child_processes;
        std::vector<process_output> _output;
        sys::event_poller _poller;
        std::thread _output_thread;
        exit_code _exitcode = exit_code::all;
        duration _execution_delay = duration::zero();
        char** _argv = nullptr;
        bool _restart = false;
        std::atomic<bool> _stopped{false};
        test_queue _tests;
        line_array _lines;
        bool _no_tests = false;
        bool _tests_succeeded = false;

    public:

        application() = default;
        inline explicit application(int argc, char* argv[]) { this->init(argc, argv); }

        void usage();
        void init(int argc, char* argv[]);
        void run();
        int wait();

        inline void send(sys::signal s) { this->_child_processes.send(s); }
        inline void terminate() { this->send(sys::signal::terminate); }
        inline bool stopped() { return this->_stopped; }
        inline bool will_restart() const noexcept { return this->_restart; }

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("dtest", args...);
        }

        template <class Function>
        void run_process(std::string node_name, Function child_main) {
            auto& node = this->_cluster.node(node_name);
            this->_child_processes.emplace([&] () {
                sys::this_process::enter(node.network_namespace().fd());
                sys::this_process::enter(node.hostname_namespace().fd());
                return child_main();
            });
        }

    private:

        int accumulate_return_value();
        void process_events();
        bool run_tests();

    };

}

#endif // vim:filetype=cpp
