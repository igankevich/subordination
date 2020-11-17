#include <iostream>
#include <regex>
#include <sstream>
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
        "dtest [-h] [--help] [--exit-code code] [--restart] [--name name] [--size n]\n"
        "      [--network ip/prefix] [--peer-network ip/prefix]\n"
        "      [--exec where command argument1...]\n"
        "--exit-code code      how exit code of child processes is accumulated,\n"
        "                      possible values: all, master, process no. starting from 1\n"
        "--restart             restart test when it finishes (useful when testing power outage)\n"
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
    this->_argv = argv;
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
            this->_exit_code = to_exit_code(argv[++i]);
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
        } else if (arg == "--restart") {
            this->_will_restart = true;
        } else {
            std::stringstream tmp;
            tmp << "unknown argument: " << arg;
            throw std::invalid_argument(tmp.str());
        }
    }
    this->_cluster.network(cluster_network);
    this->_cluster.peer_network(cluster_peer_network);
    this->_cluster.generate_nodes(cluster_size);
}

void dts::application::add_process(cluster_node_bitmap nodes, sys::argstream args) {
    this->_where.emplace_back(std::move(nodes));
    this->_arguments.emplace_back(std::move(args));
}

void dts::application::run_process(cluster_node_bitmap where, sys::argstream args) {
    const auto num_nodes = this->_cluster.size();
    for (size_t i=0; i<num_nodes; ++i) {
        if (!where.matches(i)) { continue; }
        auto& node = this->_cluster.nodes()[i];
        sys::pipe stdout, stderr;
        stdout.out().unsetf(sys::open_flag::non_blocking);
        stderr.out().unsetf(sys::open_flag::non_blocking);
        {
            std::stringstream tmp;
            tmp << node.name() << ": ";
            std::copy(args.argv(), args.argv() + args.argc(),
                      sys::intersperse_iterator<char*,char>(tmp, ' '));
            this->log("_", tmp.str());
        }
        this->_child_processes.emplace([&] () {
            stdout.in().close();
            stderr.in().close();
            sys::fildes out(STDOUT_FILENO);
            out = stdout.out();
            sys::fildes err(STDERR_FILENO);
            err = stderr.out();
            sys::this_process::enter(node.network_namespace().fd());
            sys::this_process::enter(node.hostname_namespace().fd());
            return sys::this_process::execute(args.argv());
        });
        this->_child_process_nodes.emplace_back(i);
        stdout.out().close();
        stderr.out().close();
        lock_type lock(this->_mutex);
        this->_output.emplace_back(node.veth().name()+": ", std::move(stdout.in()), 1);
        this->_poller.emplace(this->_output.back().in().fd(), sys::event::in);
        this->_output.emplace_back(node.veth().name()+": ", std::move(stderr.in()), 2);
        this->_poller.emplace(this->_output.back().in().fd(), sys::event::in);
        this->_poller.notify_one();
    }
    this->_where.emplace_back(std::move(where));
    this->_arguments.emplace_back(std::move(args));
}

void dts::application::kill_process(cluster_node_bitmap where, sys::signal signal) {
    const auto num_processes = this->_arguments.size();
    for (size_t i=0; i<num_processes; ++i) {
        if (i >= this->_child_process_nodes.size()) { continue; }
        auto node = this->_child_process_nodes[i];
        if (!where.matches(node)) { continue; }
        auto& process = this->_child_processes[i];
        log("send _ to child process _ running on node _", signal, process.id(), node);
        process.send(signal);
    }
}

int dts::application::wait() {
    int retval = 0;
    if (!this->_no_tests) {
        this->_tests_completed.get_future().get();
    }
    if (this->_exit_code == exit_code_type::all ||
        this->_exit_code == exit_code_type::none) {
        retval = accumulate_return_value();
        if (this->_exit_code == exit_code_type::none) {
            retval = 0;
        }
    } else if (this->_exit_code == exit_code_type::master) {
        auto nprocs = this->_child_processes.size();
        // wait for master process
        if (nprocs > 0 && this->_child_processes.front()) {
            auto stat = this->_child_processes.front().wait();
            this->log("master process terminated: _", stat);
            retval = stat.exit_code() | sys::signal_type(stat.term_signal());
        }
        // terminate child processes
        if (nprocs > 1) {
            for (size_t i=1; i<nprocs; ++i) {
                auto& proc = this->_child_processes[i];
                if (proc) { proc.terminate(); }
                auto stat = proc.wait();
                this->log("slave process terminated: _", stat);
            }
        }
    } else {
        size_t proc_no = static_cast<size_t>(this->_exit_code);
        auto nprocs = this->_child_processes.size();
        if (proc_no < nprocs) {
            auto status = this->_child_processes[proc_no].wait();
            this->log("process #_ terminated: _", proc_no, status);
            retval = status.exit_code() | sys::signal_type(status.term_signal());
        }
        for (size_t i=0; i<nprocs; ++i) {
            if (i == proc_no) { continue; }
            auto& proc = this->_child_processes[i];
            if (proc) { proc.terminate(); }
            auto stat = proc.wait();
            this->log("process #_ terminated: _", i, stat);
        }
    }
    this->_stopped = true;
    this->_poller.notify_one();
    if (this->_output_thread.joinable()) { this->_output_thread.join(); }
    if (this->_no_tests) { return retval; }
    return this->_tests_succeeded ? 0 : 1;
}

int dts::application::accumulate_return_value() {
    int ret = 0;
    for (auto& proc : this->_child_processes) {
        auto stat = proc.wait();
        this->log("child process terminated: _", stat);
        ret |= stat.exit_code() | sys::signal_type(stat.term_signal());
    }
    return ret;
}

void dts::application::run() {
    this->_no_tests = this->_tests.empty();
    if (this->_cluster.size() == 1) {
        { sys::network_interface lo("lo"); lo.setf(sys::network_interface::flag::up); }
        for (const auto& a : this->_arguments) {
            this->_child_processes.emplace([&a] () {
                return sys::this_process::execute(a.argv());
            });
        }
    } else {
        auto& nodes = this->_cluster.nodes();
        auto num_nodes = nodes.size();
        auto num_processes = this->_arguments.size();
        using f = sys::network_interface::flag;
        using sys::this_process::execute;
        using sys::this_process::enter;
        std::vector<sys::two_way_pipe> pipes;
        for (size_t i=0; i<num_nodes; ++i) {
            auto& node = nodes[i];
            node.veth(sys::veth_interface(node.name(), 'v'+node.name()));
            auto& veth = node.veth();
            bool first_process = true;
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
                using pf = sys::process_flag;
                this->_child_processes.emplace([&] () {
                    try {
                        stdout.in().close();
                        stderr.in().close();
                        sys::fildes out(STDOUT_FILENO);
                        out = stdout.out();
                        sys::fildes err(STDERR_FILENO);
                        err = stderr.out();
                        if (!first_process) {
                            enter(node.network_namespace().fd());
                            enter(node.hostname_namespace().fd());
                        }
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
                        this->log("child _ delay _ms pid _", args.argv()[0],
                                  duration_cast<milliseconds>(delay).count(),
                                  sys::this_process::id());
                        std::this_thread::sleep_for(delay);
                        sys::this_process::execute(args.argv());
                    } catch (const std::exception& err) {
                        this->log("child _ _", j+1, err.what());
                        return 1;
                    }
                    return 0;
                }, pf::signal_parent | pf::unshare_network | pf::unshare_hostname);
                this->_child_process_nodes.emplace_back(i);
                pipe.close_in_parent();
                stdout.out().close();
                stderr.out().close();
                this->_output.emplace_back(veth.name()+": ", std::move(stdout.in()), 1);
                this->_output.emplace_back(veth.name()+": ", std::move(stderr.in()), 2);
                auto& proc = this->_child_processes.back();
                node.network_namespace(proc.get_namespace("net"));
                node.hostname_namespace(proc.get_namespace("uts"));
                if (first_process) {
                    auto index = veth.peer().index();
                    veth.peer().set_namespace(node.network_namespace().fd());
                    node.run([&] () {
                        sys::network_interface peer(index);
                        peer.address(node.peer_interface_address());
                        peer.up();
                        if (!sys::u16(peer.flags() & f::up)) {
                            std::stringstream tmp;
                            tmp << "veth " << peer.name() << " is down";
                            throw std::runtime_error(tmp.str());
                        }
                    });
                    veth.address(node.interface_address());
                    veth.up();
                    first_process = false;
                }
                pipe.parent_out().write("x", 1);
                pipes.emplace_back(std::move(pipe));
            }
        }
        sys::bridge_interface br(this->_cluster.name());
        for (const auto& node : nodes) { br.add(node.veth()); }
        br.up();
        for (const auto& node : nodes) {
            using f = sys::network_interface::flag;
            if (!sys::u16(node.veth().flags() & f::up)) {
                std::stringstream tmp;
                tmp << "veth " << node.veth().name() << " is down";
                throw std::runtime_error(tmp.str());
            }
        }
        // launch all processes simultaneously
        for (auto& pipe : pipes) { pipe.parent_out().write("x", 1); }
        this->_cluster.bridge(std::move(br));
        for (const auto& output : this->_output) {
            this->_poller.emplace(output.in().fd(), sys::event::in);
        }
        this->_output_thread = std::thread([this] () { process_events(); });
    }
}

void dts::application::process_events() {
    try {
        using namespace sys::this_process;
        ignore_signal(sys::signal::broken_pipe);
        lock_type lock(this->_mutex);
        this->_poller.wait(lock, [this,&lock] () {
            auto pipe_fd = this->_poller.pipe_in();
            for (const auto& event : this->_poller) {
                if (event.fd() == pipe_fd) { continue; }
                for (auto& output : this->_output) {
                    //if (event.fd() == output.in().fd()) { output.copy(); }
                    output.copy(this->_lines);
                }
            }
            if (!this->_no_tests) {
                if (!this->_tests_succeeded && run_tests()) {
                    this->_tests_succeeded = true;
                    this->_tests_completed.set_value();
                    this->send(sys::signal::terminate);
                }
            }
            return stopped();
        });
    } catch (const std::exception& err) {
        log("output _", err.what());
    }
}

bool dts::application::run_tests() {
    while (!this->_tests.empty()) {
        auto& test = this->_tests.front();
        try {
            test(*this, this->_lines);
            std::cerr << "========================================\n";
            std::cerr << test.description() << '\n';
            std::cerr << "Completed successfully.\n";
            std::cerr << "========================================\n";
        } catch (const std::exception& err) {
            std::cerr << "========================================\n";
            std::cerr << test.description() << '\n';
            std::cerr << err.what();
            std::cerr << "========================================\n";
            break;
        }
        this->_tests.pop();
    }
    if (this->_tests.empty()) {
        std::cerr << "========================================\n";
        std::cerr << "All tests completed successfully.\n";
        std::cerr << "========================================\n";
    }
    return this->_tests.empty();
}

void dts::process_output::copy(string_array& lines) {
    auto& buf = this->_buffer;
    buf.fill(this->_in);
    buf.flip();
    // limit to newline character
    auto first = buf.data();
    auto last = first + buf.limit();
    auto old_limit = buf.limit();
    // print evey line
    auto old_first = first;
    auto prev = first;
    while (first != last) {
        if (*first == '\n') {
            buf.limit(first-old_first+1);
            lines.emplace_back();
            lines.back().reserve(this->_prefix.size() + first-prev);
            lines.back().append(this->_prefix);
            lines.back().append(prev, first);
            this->_out.write(this->_prefix.data(), this->_prefix.size());
            buf.flush(this->_out);
            prev = first+1;
        }
        ++first;
    }
    buf.limit(old_limit);
    buf.compact();
    std::clog << std::flush;
}

namespace  {

    dts::application* aptr{};
    sys::pid_type child_process_id = 0;

    void child_signal_handlers() {
        using namespace sys::this_process;
        auto on_terminate = sys::signal_action([](int sig) {
            const auto is_alarm = sys::signal(sig) == sys::signal::alarm;
            if (aptr) {
                try {
                    aptr->send(is_alarm ? sys::signal::kill : sys::signal::terminate);
                } catch (const std::exception& err) {
                    ::write(2, err.what(), std::string::traits_type::length(err.what()));
                }
            }
            if (!is_alarm) { ::alarm(3); }
        });
        using s = sys::signal;
        ignore_signal(s::broken_pipe);
        bind_signal(s::terminate, on_terminate);
        bind_signal(s::keyboard_interrupt, on_terminate);
        bind_signal(s::alarm, on_terminate);
    }

    void parent_signal_handlers() {
        using namespace sys::this_process;
        using s = sys::signal;
        auto on_terminate = sys::signal_action([](int) {
            if (child_process_id) {
                try {
                    sys::process_view(child_process_id).send(s::terminate);
                } catch (const std::exception& err) {
                    const char* msg = "error in parent signal handler";
                    ::write(2, msg, std::string::traits_type::length(msg));
                    ::write(2, err.what(), std::string::traits_type::length(err.what()));
                }
            }
        });
        ignore_signal(s::broken_pipe);
        bind_signal(s::keyboard_interrupt, on_terminate);
        bind_signal(s::terminate, on_terminate);
    }

    int nested_run(dts::application& app) {
        using namespace dts;
        sys::pipe pipe;
        pipe.in().unsetf(sys::open_flag::non_blocking);
        pipe.out().unsetf(sys::open_flag::non_blocking);
        using pf = sys::process_flag;
        sys::process child([&pipe,&app] () {
            try {
                child_signal_handlers();
                pipe.out().close();
                char ch;
                pipe.in().read(&ch, 1);
                app.run();
                return app.wait();
            } catch (const std::exception& err) {
                app.terminate();
                std::cerr << err.what() << std::endl;
                return 1;
            }
        },
        pf::unshare_users | pf::unshare_network | pf::unshare_hostname | pf::signal_parent,
        4096*10);
        child_process_id = child.id();
        child.init_user_namespace();
        pipe.in().close();
        pipe.out().write("x", 1);
        return child.wait().exit_code();
    }

}

int dts::run(application& app) {
    ::aptr = &app;
    parent_signal_handlers();
    auto ret = nested_run(app);
    if (app.will_restart()) {
        ::setenv("DTEST_NO_RESTART", "", 1);
        app.log("restart after power failure");
        app.will_restart(false);
        return nested_run(app);
    }
    app.log("terminated");
    return ret;
}

void dts::expect_event_sequence(const string_array& lines, const string_array& regex_strings) {
    std::vector<std::regex> expressions;
    for (const auto& s : regex_strings) { expressions.emplace_back(s); }
    auto first = expressions.begin();
    auto last = expressions.end();
    auto first2 = lines.begin();
    auto last2 = lines.end();
    while (first != last && first2 != last2) {
        if (std::regex_match(*first2, *first)) { ++first; }
        ++first2;
    }
    if (first != last) {
        std::stringstream msg;
        msg << "unmatched expressions: \n";
        size_t offset = first - expressions.begin();
        std::copy(
            regex_strings.begin() + offset,
            regex_strings.end(),
            std::ostream_iterator<std::string>(msg, "\n")
        );
        throw std::runtime_error(msg.str());
    }
}

void dts::expect_event_count(const string_array& lines,
                             std::string regex_string,
                             size_t expected_count) {
    std::regex expr(regex_string);
    size_t count = 0;
    for (const auto& line : lines) {
        if (std::regex_match(line, expr)) { ++count; }
    }
    if (count != expected_count) {
        std::stringstream msg;
        msg << "bad event count: expected=" << expected_count << ",actual=" << count;
        throw std::runtime_error(msg.str());
    }
}
