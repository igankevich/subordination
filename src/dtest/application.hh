#ifndef DTEST_APPLICATION_HH
#define DTEST_APPLICATION_HH

#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/process_group>

#include <dtest/cluster.hh>
#include <dtest/cluster_node_bitmap.hh>
#include <dtest/cluster_node.hh>
#include <dtest/exit_code.hh>

namespace dts {

    class application {

    private:
        cluster _cluster;
        std::vector<sys::argstream> _arguments;
        std::vector<cluster_node_bitmap> _where;
        std::vector<cluster_node> _nodes;
        sys::process_group _procs;
        exit_code _exitcode = exit_code::all;

    public:

        application() = default;
        inline explicit application(int argc, char* argv[]) { this->init(argc, argv); }

        void usage();
        void init(int argc, char* argv[]);
        void run();
        int wait();

        inline void send(sys::signal s) { this->_procs.send(s); }
        inline void terminate() { this->send(sys::signal::terminate); }

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
