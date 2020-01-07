#include <iostream>
#include <string>

#include <unistdx/base/command_line>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/execute>
#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/bridge_interface>
#include <unistdx/net/interface_address>
#include <unistdx/net/network_interface>
#include <unistdx/net/veth_interface>

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
    bool help = false;
    size_t cluster_size = 1;
    std::string cluster_name = "a";
    cluster_node::address_type cluster_network{{10,1,0,1},16};
    cluster_node::address_type cluster_peer_network{{10,0,0,1},16};
    bool inside_exec = false;
    bool exec_first_arg = false;
    sys::input_operator_type options[] = {
        sys::ignore_first_argument(),
        [this,&help,&inside_exec] (int pos, const std::string& arg) {
            bool success = false;
            if (inside_exec) { return success; }
            if (arg == "--help" || arg == "-h") { help = true; success = true; }
            return success;
        },
        [&] (int pos, const std::string& arg) {
            if (arg == "--exec") {
                this->_arguments.emplace_back();
                inside_exec = true;
                exec_first_arg = true;
                return true;
            }
            if (inside_exec) {
                if (exec_first_arg) {
                    this->_where.emplace_back(cluster_size);
                    this->_where.back().read(arg);
                    exec_first_arg = false;
                } else if (arg == "--") {
                    inside_exec = false;
                } else {
                    this->_arguments.back().append(arg);
                }
                return true;
            }
            return false;
        },
        sys::make_key_value("--exit-code", _exitcode),
        sys::make_key_value("--name", cluster_name),
        sys::make_key_value("--size", cluster_size),
        sys::make_key_value("--network", cluster_network),
        sys::make_key_value("--peer-network", cluster_peer_network),
        nullptr
    };
    sys::parse_arguments(argc, argv, options);
    if (help) { usage(); std::exit(0); }
    this->_cluster.name(cluster_name);
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
    return retval;
}

int dts::application::accumulate_return_value() {
    int ret = 0;
    for (auto& proc : this->_procs) {
        auto stat = proc.wait();
        this->log("child process terminated: _", stat);
        return ret | stat.exit_code() | sys::signal_type(stat.term_signal());
    }
    return ret;
}

void dts::application::run() {
    using unshare = sys::unshare_flag;
    sys::this_process::unshare(unshare::users | unshare::network);
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
        sys::bridge_interface br(this->_cluster.name());
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
        sys::fildes parent_netns = sys::this_process::get_namespace("net");
        br.up();
        for (size_t i=0; i<num_nodes; ++i) {
            const auto& node = nodes[i];
            veths.emplace_back(node.name(), 'v'+node.name());
            auto& veth = veths.back();
            sys::fildes netns;
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
                this->_procs.emplace([&] () {
                    try {
                        if (!first_process) {
                            sys::this_process::enter(netns.fd());
                        }
                        pipe.close_in_child();
                        std::stringstream tmp;
                        tmp << node.peer_interface_address();
                        ::setenv("DTEST_INTERFACE_ADDRESS", tmp.str().data(), 1);
                        //pipe.child_out().write("x", 1);
                        char ch;
                        pipe.child_in().read(&ch, 1);
                        if (first_process) {
                            //sys::network_interface lo("lo");
                            //using f = sys::network_interface::flag;
                            //lo.setf(f::up | f::running);
                            //veth.peer().address(node.peer_interface_address());
                        }
                        sys::this_process::hostname(veth.name());
                        //pipe.child_in().read(&ch, 1);
                        sys::this_process::execute(args.argv());
                        //usleep(1000000);
                        //sys::this_process::execute({
                        //    "/run/current-system/profile/sbin/ip",
                        //    "a"}
                        //);
                    } catch (const std::exception& err) {
                        sys::log_message("child", "_ _", j+1, err.what());
                        return 1;
                    }
                    return 0;
                }, sys::process_flag(SIGCHLD | CLONE_NEWUTS | CLONE_NEWNET));
                pipe.close_in_parent();
                auto& proc = this->_procs.back();
                if (first_process) {
                    netns = proc.get_namespace("net");
                    //auto index0 = veth.index();
                    auto index = veth.peer().index();
                    veth.peer().set_namespace(netns.fd());
                    sys::this_process::enter(netns.fd());
                    sys::network_interface peer(index);
                    peer.address(node.peer_interface_address());
                    peer.up();
                    sys::this_process::enter(parent_netns.fd());
                    veth.address(node.interface_address());
                    veth.up();
                    br.add(veth);
                    first_process = false;
                }
                pipe.parent_out().write("x", 1);
            }
        }
        for (const auto& veth : veths) {
            using f = sys::network_interface::flag;
            if (!sys::u16(veth.flags() & f::up)) {
                std::stringstream tmp;
                tmp << "veth " << veth.name() << " is down";
                throw std::runtime_error(tmp.str());
            }
        }
    }
}
