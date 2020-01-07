#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <unistdx/base/command_line>
#include <unistdx/base/ios_guard>
#include <unistdx/base/log_message>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/bridge_interface>
#include <unistdx/net/interface_address>
#include <unistdx/net/network_interface>
#include <unistdx/net/veth_interface>

enum struct Exit_code {Master, All};

const char*
to_string(Exit_code rhs) {
    switch (rhs) {
        case Exit_code::Master: return "master";
        case Exit_code::All: return "all";
        default: return "unknown";
    }
}

std::ostream&
operator<<(std::ostream& out, const Exit_code& rhs) {
    return out << to_string(rhs);
}

std::istream&
operator>>(std::istream& in, Exit_code& rhs) {
    std::string tmp;
    in >> tmp;
    if (tmp == to_string(Exit_code::Master)) {
        rhs = Exit_code::Master;
    } else
    if (tmp == to_string(Exit_code::All)) {
        rhs = Exit_code::All;
    }
    return in;
}

class Node {

public:
    using address_type = sys::interface_address<sys::ipv4_address>;

private:
    std::string _name;
    address_type _address;
    address_type _peer_address;

public:
    inline const std::string& name() const { return this->_name; }
    inline const address_type& interface_address() const { return this->_address; }
    inline const address_type& peer_interface_address() const { return this->_peer_address; }
    inline void name(const std::string& name) { this->_name = name; }
    inline void interface_address(const address_type& rhs) { this->_address = rhs; }
    inline void peer_interface_address(const address_type& rhs) { this->_peer_address = rhs; }

};

class Cluster {

public:
    using address_type = Node::address_type;

private:
    std::string _name;
    address_type _network;
    address_type _peer_network;
    std::vector<Node> _nodes;

public:
    inline const std::vector<Node>& nodes() const { return this->_nodes; }
    inline const std::string& name() const { return this->_name; }
    inline void name(const std::string& name) { this->_name = name; }
    inline void size(size_t n) { this->_nodes.resize(n); }
    inline size_t size() const { return this->_nodes.size(); }
    inline void network(address_type rhs) { this->_network = rhs; }
    inline void peer_network(address_type rhs) { this->_peer_network = rhs; }
    inline void generate() {
        auto num_nodes = this->_nodes.size();
        auto num_addresses = this->_network.count()-2;
        if (num_addresses < num_nodes) {
            throw std::invalid_argument("insufficient network size");
        }
        size_t n = 0;
        auto address = this->_network.begin();
        auto peer_address = this->_peer_network.begin();
        auto ndigits = std::to_string(this->_nodes.size()).size();
        for (auto& node : this->_nodes) {
            ++n;
            std::stringstream tmp;
            tmp << name() << std::setw(ndigits) << std::setfill('0') << n;
            node.name(tmp.str());
            node.interface_address({*address++,this->_network.netmask()});
            node.peer_interface_address({*peer_address++,this->_peer_network.netmask()});
            std::clog << "node.name()=" << node.name() << std::endl;
            std::clog << "node.interface_address()=" << node.interface_address() << std::endl;
            std::clog << "node.peer_interface_address()=" << node.peer_interface_address() << std::endl;
        }
    }

};

class Where {

private:
    std::vector<bool> _nodes;

public:

    inline explicit Where(size_t n): _nodes(n, false) {}
    inline bool matches(size_t node_number) const { return this->_nodes[node_number]; }

    inline void read(std::string arg) {
        if (arg == "all" || arg == "*") {
            std::fill(this->_nodes.begin(), this->_nodes.end(), true);
        } else {
            arg += ',';
            std::string tmp;
            for (auto ch : arg) {
                if (ch == ',') {
                    std::stringstream s(tmp);
                    size_t n = 0;
                    if (!(s >> n) || n == 0 || n > this->_nodes.size()) {
                        throw std::invalid_argument("where");
                    }
                    --n;
                    this->_nodes[n] = true;
                    tmp.clear();
                } else { tmp += ch; }
            }
        }
    }

};

struct Executor {

private:
    Cluster _cluster;
    std::vector<sys::argstream> _arguments;
    std::vector<Where> _where;
    sys::process_group _procs;
    Exit_code _exitcode = Exit_code::All;

public:

    Executor() = default;

    Executor(int argc, char* argv[]) {
        this->init(argc, argv);
    }

    void usage(const char* name) {
        std::cout << name << " [-h] [--help] [--exit-code what] \n"
            "[--cluster-name name] [--cluster-size n] "
            "[--cluster-network ip/prefix] [--cluster-peer-network ip/prefix]\n"
            "[--exec where command argument1...]\n"
            "    --exit-code code    all,master\n";
    }

    void
    init(int argc, char* argv[]) {
        bool help = false;
        size_t cluster_size = 1;
        std::string cluster_name = "a";
        Node::address_type cluster_network{{10,1,0,1},16};
        Node::address_type cluster_peer_network{{10,0,0,1},16};
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
            sys::make_key_value("--cluster-name", cluster_name),
            sys::make_key_value("--cluster-size", cluster_size),
            sys::make_key_value("--cluster-network", cluster_network),
            sys::make_key_value("--cluster-peer-network", cluster_peer_network),
            nullptr
        };
        sys::parse_arguments(argc, argv, options);
        if (help) { usage(argv[0]); std::exit(0); }
        this->_cluster.name(cluster_name);
        this->_cluster.size(cluster_size);
        this->_cluster.network(cluster_network);
        this->_cluster.peer_network(cluster_peer_network);
        this->_cluster.generate();
        assert(!_arguments.empty());
    }

    void
    execute() {
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
                    std::clog << node.name() << ": ";
                    std::copy(
                        args.argv(), args.argv() + args.argc(),
                        sys::intersperse_iterator<char*,char>(std::clog, ' ')
                    );
                    std::clog << '\n';
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
                            ::setenv("TEST_RUNNER_INTERFACE_ADDRESS", tmp.str().data(), 1);
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
                        std::clog << "index=" << index << std::endl;
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
            sys::process p{[] () {
                usleep(1000000);
                sys::this_process::execute({"/run/current-system/profile/sbin/ip", "a"});
            }};
            p.wait();
            /*
            for (const auto& veth : veths) {
                std::clog << "veth.flags()=" << std::bitset<32>(int(veth.flags())) << std::endl;
            }
            */
        }
    }

    int
    wait() {
        int retval = 0;
        if (_exitcode == Exit_code::All) {
            retval = accumulate_return_value(_procs.begin(), _procs.end());
        } else if (_exitcode == Exit_code::Master) {
            // wait for master process
            if (_procs.size() > 0 && _procs.front()) {
                sys::process_status stat = _procs.front().wait();
                std::clog << "master process terminated: " << stat << std::endl;
                retval = stat.exit_code() | sys::signal_type(stat.term_signal());
            }
            // terminate child processes
            if (_procs.size() > 1) {
                std::for_each(
                    _procs.begin()+1, _procs.end(),
                    [&retval] (sys::process& rhs) {
                        if (rhs) {
                            rhs.terminate();
                        }
                        sys::process_status stat = rhs.wait();
                        std::clog << "child process terminated: " << stat << std::endl;
                    }
                );
            }
        }
        return retval;
    }

    void
    send(sys::signal s) {
        this->_procs.send(s);
    }

    friend std::ostream&
    operator<<(std::ostream& out, const Executor& rhs) {
        out << "Exit_code: " << rhs._exitcode << '\n';
        int i = 0;
        std::for_each(
            rhs._arguments.begin(), rhs._arguments.end(),
            [&out,&i] (const sys::argstream& rhs) {
                {
                    sys::ios_guard g(out);
                    out << "Command #"
                        << std::setw(5) << std::setfill('0') << std::right
                        << ++i << ": ";
                }
                std::copy(
                    rhs.argv(), rhs.argv() + rhs.argc(),
                    sys::intersperse_iterator<char*,char>(out, ' ')
                );
                out << '\n';
            }
        );
        return out;
    }

private:

    template<class Iterator>
    int
    accumulate_return_value(Iterator first, Iterator last) {
        return std::accumulate(
            first, last, 0,
            [] (int ret, sys::process& rhs) {
                sys::process_status stat = rhs.wait();
                std::clog << "child process terminated: " << stat << std::endl;
                return ret | stat.exit_code() | sys::signal_type(stat.term_signal());
            }
        );
    }

};

Executor exe;

void
on_terminate(int sig) {
    exe.send(sys::signal::terminate);
    ::alarm(3);
}

void
on_alarm(int) {
    exe.send(sys::signal::kill);
}

void
init_signal_handlers() {
    using namespace sys::this_process;
    ignore_signal(sys::signal::broken_pipe);
    bind_signal(sys::signal::keyboard_interrupt, on_terminate);
    bind_signal(sys::signal::terminate, on_terminate);
    bind_signal(sys::signal::alarm, on_alarm);
}

int main(int argc, char* argv[]) {
    init_signal_handlers();
    exe.init(argc, argv);
    std::clog << exe << std::flush;
    exe.execute();
    return exe.wait();
}
