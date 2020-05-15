#include <iostream>
#include <string>

#include <unistdx/base/command_line>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/interface_address>
#include <unistdx/net/network_interface>

#include <dtest/application.hh>

void dts::application::usage() {
    std::cout <<
        "dtest [-h] [--help] [--exit-code code] [--name name] [--size n]\n"
        "      [--network ip/prefix] [--peer-network ip/prefix]\n"
        "      [--exec where command argument1...]\n"
        "--exit-code code      how exit code of child processes is accumulated,\n"
        "                      possible values: all, master\n"
        "--name name           cluster name\n"
        "--size n              the number of nodes in he cluster\n"
        "--network ip/n        subnetwork veth (default is 10.1.0.0/16)\n"
        "--peer-network ip/n   subnetwork veth peers (default is 10.0.0.0/16),\n"
        "                      this is the network where applications are executed\n"
        "--exec where args...  execute application on a set of nodes,\n"
        "                      \"where\" is a comma-separated list of node numbers\n"
        "                      starting from 1 (e.g. 1,2,4) or \"*\",\n"
        "                      \"args\" is argument list which is forwared to exec()\n"
        "                      without any modifications\n";
}

void dts::application::init(int argc, char* argv[]) {
    size_t cluster_size = 1;
    cluster_node::address_type cluster_network{{10,1,0,1},16};
    cluster_node::address_type cluster_peer_network{{10,0,0,1},16};
    bool inside_exec = false;
    bool exec_first_arg = false;
    for (int i=1; i<argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--exec") {
            this->_arguments.emplace_back();
            inside_exec = true;
            exec_first_arg = true;
        } else if (inside_exec) {
            if (exec_first_arg) {
                this->_where.emplace_back(cluster_size);
                this->_where.back().read(arg);
                exec_first_arg = false;
            } else if (arg == "--") {
                inside_exec = false;
            } else {
                this->_arguments.back().append(arg);
            }
        } else if (arg == "--help" || arg == "-h") {
             usage(); std::exit(0);
        } else if (arg == "--exit-code") {
            if (i+1 == argc) { throw std::invalid_argument("bad --exit-code"); }
            this->_exitcode = to_exit_code(argv[++i]);
        } else if (arg == "--name") {
            if (i+1 == argc) { throw std::invalid_argument("bad --name"); }
            this->_cluster.name(argv[++i]);
        } else if (arg == "--size") {
            if (i+1 == argc) { throw std::invalid_argument("bad --size"); }
            std::stringstream tmp(argv[++i]);
            tmp >> cluster_size;
            if (!tmp || cluster_size == 0) { throw std::invalid_argument("bad --size"); }
        } else if (arg == "--network") {
            if (i+1 == argc) { throw std::invalid_argument("bad --network"); }
            std::stringstream tmp(argv[++i]);
            tmp >> cluster_network;
            if (!tmp) { throw std::invalid_argument("bad --network"); }
        } else if (arg == "--peer-network") {
            if (i+1 == argc) { throw std::invalid_argument("bad --peer-network"); }
            std::stringstream tmp(argv[++i]);
            tmp >> cluster_peer_network;
            if (!tmp) { throw std::invalid_argument("bad --peer-network"); }
        } else if (arg == "--exec-delay") {
            if (i+1 == argc) { throw std::invalid_argument("bad --exec-delay"); }
            duration::rep ms{};
            std::stringstream tmp(argv[++i]);
            tmp >> ms;
            if (!tmp || ms < 0) { throw std::invalid_argument("bad --exec-delay"); }
            this->_execution_delay = std::chrono::milliseconds(ms);
        } else {
            std::stringstream tmp;
            tmp << "unknown argument: " << arg;
            throw std::invalid_argument(tmp.str());
        }
    }
    this->_cluster.size(cluster_size);
    this->_cluster.network(cluster_network);
    this->_cluster.peer_network(cluster_peer_network);
    this->_nodes = this->_cluster.nodes();
}

int dts::application::wait() {
    int retval = 0;
    if (this->_exitcode == exit_code::all) {
        retval = accumulate_return_value();
    } else if (this->_exitcode == exit_code::master) {
        auto nprocs = this->_procs.size();
        // wait for master process
        if (nprocs > 0 && this->_procs.front()) {
            auto stat = this->_procs.front().wait();
            this->log("master process terminated: _", stat);
            retval = stat.exit_code() | sys::signal_type(stat.term_signal());
        }
        // terminate child processes
        if (nprocs > 1) {
            for (size_t i=1; i<nprocs; ++i) {
                auto& proc = this->_procs[i];
                if (proc) { proc.terminate(); }
                auto stat = proc.wait();
                this->log("slave process terminated: _", stat);
            }
        }
    }
    this->_stopped = true;
    this->_poller.notify_one();
    if (this->_output_thread.joinable()) { this->_output_thread.join(); }
    return retval;
}

int dts::application::accumulate_return_value() {
    int ret = 0;
    for (auto& proc : this->_procs) {
        auto stat = proc.wait();
        this->log("child process terminated: _", stat);
        ret |= stat.exit_code() | sys::signal_type(stat.term_signal());
    }
    return ret;
}


void dts::application::run() {
    if (this->_cluster.size() == 1) {
        { sys::network_interface lo("lo"); lo.setf(sys::network_interface::flag::up); }
        std::for_each(
            _arguments.begin(), _arguments.end(),
            [this] (const sys::argstream& rhs) {
                _procs.emplace([&rhs] () {
                    return sys::this_process::execute(rhs.argv());
                });
            }
        );
    } else {
        std::vector<sys::veth_interface> veths;
        auto nodes = this->_cluster.nodes();
        auto num_nodes = nodes.size();
        auto num_processes = this->_arguments.size();
        /*
        for (size_t i=0; i<num_nodes; ++i) {
            const auto& node = nodes[i];
            veths.emplace_back(node.name(), 'v'+node.name());
            auto& veth = veths.back();
            veth.address(node.interface_address());
            veth.up();
            br.add(veth);
            for (size_t j=0; j<num_processes; ++j) {
                const auto& where = this->_where[j];
                if (!where.matches(i)) { continue; }
                veth.peer().address(node.peer_interface_address());
                veth.peer().up();
            }
        }
        */
        using f = sys::network_interface::flag;
        using sys::this_process::execute;
        using sys::this_process::enter;
        sys::fildes parent_netns = sys::this_process::get_namespace("net");
        std::vector<sys::two_way_pipe> pipes;
        std::vector<sys::fildes> netns_fds;
        for (size_t i=0; i<num_nodes; ++i) {
            const auto& node = nodes[i];
            veths.emplace_back(node.name(), 'v'+node.name());
            auto& veth = veths.back();
            bool first_process = true;
            //veth.address(node.interface_address());
            //veth.up();
            //br.add(veth);
            for (size_t j=0; j<num_processes; ++j) {
                const auto& where = this->_where[j];
                const auto& args = this->_arguments[j];
                if (!where.matches(i)) { continue; }
                {
                    std::stringstream tmp;
                    tmp << node.name() << ": ";
                    std::copy(args.argv(), args.argv() + args.argc(),
                        sys::intersperse_iterator<char*,char>(tmp, ' '));
                    this->log("_", tmp.str());
                }
                sys::two_way_pipe pipe;
                pipe.parent_in().unsetf(sys::open_flag::non_blocking);
                pipe.parent_out().unsetf(sys::open_flag::non_blocking);
                pipe.child_in().unsetf(sys::open_flag::non_blocking);
                pipe.child_out().unsetf(sys::open_flag::non_blocking);
                sys::pipe stdout, stderr;
                stdout.out().unsetf(sys::open_flag::non_blocking);
                stderr.out().unsetf(sys::open_flag::non_blocking);
                this->_procs.emplace([&] () {
                    try {
                        stdout.in().close();
                        stderr.in().close();
                        sys::fildes out(STDOUT_FILENO);
                        out = stdout.out();
                        sys::fildes err(STDERR_FILENO);
                        err = stderr.out();
                        auto& netns = netns_fds.back();
                        if (!first_process) { enter(netns.fd()); }
                        pipe.close_in_child();
                        std::stringstream tmp;
                        tmp << node.peer_interface_address();
                        ::setenv("DTEST_INTERFACE_ADDRESS", tmp.str().data(), 1);
                        char ch;
                        pipe.child_in().read(&ch, 1);
                        sys::this_process::hostname(veth.name());
                        pipe.child_in().read(&ch, 1);
                        auto delay = this->_execution_delay*((i+1)+(j+1)*num_nodes);
                        using namespace std::chrono;
                        this->log("child delay _ _ms pid _", args.argv()[0],
                                  duration_cast<milliseconds>(delay).count(),
                                  sys::this_process::id());
                        std::this_thread::sleep_for(delay);
                        sys::this_process::execute(args.argv());
                    } catch (const std::exception& err) {
                        this->log("child _ _", j+1, err.what());
                        return 1;
                    }
                    return 0;
                }, sys::process_flag(SIGCHLD | CLONE_NEWUTS | CLONE_NEWNET));
                pipe.close_in_parent();
                stdout.out().close();
                stderr.out().close();
                this->_output.emplace_back(veth.name()+": ", std::move(stdout.in()), 1);
                this->_output.emplace_back(veth.name()+": ", std::move(stderr.in()), 2);
                auto& proc = this->_procs.back();
                if (first_process) {
                    netns_fds.emplace_back(proc.get_namespace("net"));
                    auto& netns = netns_fds.back();
                    //auto index0 = veth.index();
                    auto index = veth.peer().index();
                    veth.peer().set_namespace(netns.fd());
                    enter(netns.fd());
                    sys::network_interface peer(index);
                    peer.address(node.peer_interface_address());
                    peer.up();
                    if (!sys::u16(peer.flags() & f::up)) {
                        std::stringstream tmp;
                        tmp << "veth " << peer.name() << " is down";
                        throw std::runtime_error(tmp.str());
                    }
                    sys::this_process::enter(parent_netns.fd());
                    veth.address(node.interface_address());
                    veth.up();
                    first_process = false;
                }
                pipe.parent_out().write("x", 1);
                pipes.emplace_back(std::move(pipe));
            }
        }
        sys::bridge_interface br(this->_cluster.name());
        for (const auto& veth : veths) { br.add(veth); }
        br.up();
        for (const auto& veth : veths) {
            using f = sys::network_interface::flag;
            if (!sys::u16(veth.flags() & f::up)) {
                std::stringstream tmp;
                tmp << "veth " << veth.name() << " is down";
                throw std::runtime_error(tmp.str());
            }
        }
        // launch all processes simultaneously
        for (auto& pipe : pipes) { pipe.parent_out().write("x", 1); }
        this->_bridge = std::move(br);
        this->_veths = std::move(veths);
        for (const auto& output : this->_output) {
            this->_poller.emplace(output.in().fd(), sys::event::in);
        }
        this->_output_thread = std::thread(
            [this] () {
                try {
                    using namespace sys::this_process;
                    ignore_signal(sys::signal::broken_pipe);
                    struct No_lock { void lock() {} void unlock() {} };
                    No_lock lock;
                    this->_poller.wait(lock, [this] () {
                        auto pipe_fd = this->_poller.pipe_in();
                        for (const auto& event : this->_poller) {
                            if (event.fd() == pipe_fd) { this->_stopped = true; }
                            for (auto& output : this->_output) {
                                //if (event.fd() == output.in().fd()) { output.copy(); }
                                output.copy();
                            }
                        }
                        return stopped();
                    });
                } catch (const std::exception& err) {
                    log("output _", err.what());
                }
            }
        );
    }
}

void dts::process_output::copy() {
    auto& buf = this->_buffer;
    buf.fill(this->_in);
    buf.flip();
    // limit to newline character
    auto first = buf.data();
    auto last = first + buf.limit();
    auto old_limit = buf.limit();
    // print evey line
    auto old_first = first;
    while (first != last) {
        if (*first == '\n') {
            buf.limit(first-old_first+1);
            this->_out.write(this->_prefix.data(), this->_prefix.size());
            buf.flush(this->_out);
        }
        ++first;
    }
    buf.limit(old_limit);
    buf.compact();
    std::clog << std::flush;
}
