#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <iomanip>
#include <numeric>

#include <unistdx/base/command_line>
#include <unistdx/base/ios_guard>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/network_interface>

enum struct Strategy {
    Master_slave,
    Peer_to_peer
};

const char*
to_string(Strategy rhs) {
    switch (rhs) {
        case Strategy::Master_slave: return "master-slave";
        case Strategy::Peer_to_peer: return "peer-to-peer";
        default: return "unknown";
    }
}

std::ostream&
operator<<(std::ostream& out, const Strategy& rhs) {
    return out << to_string(rhs);
}

std::istream&
operator>>(std::istream& in, Strategy& rhs) {
    std::string tmp;
    in >> tmp;
    if (tmp == to_string(Strategy::Master_slave)) {
        rhs = Strategy::Master_slave;
    } else
    if (tmp == to_string(Strategy::Peer_to_peer)) {
        rhs = Strategy::Peer_to_peer;
    }
    return in;
}

struct Executor {

    Executor() = default;

    Executor(int argc, char* argv[]) {
        this->init(argc, argv);
    }

    void
    init(int argc, char* argv[]) {
        sys::input_operator_type options[] = {
            sys::ignore_first_argument(),
            sys::make_key_value("--strategy", _strategy),
            [this] (int pos, const std::string& arg) {
                if (arg == "--exec") {
                    this->_arguments.emplace_back();
                } else {
                    this->_arguments.back().append(arg);
                }
                return true;
            },
            nullptr
        };
        sys::parse_arguments(argc, argv, options);
        assert(!_arguments.empty());
    }

    void
    execute() {
        using unshare = sys::unshare_flag;
        sys::this_process::unshare(unshare::users | unshare::network);
        { sys::network_interface lo("lo"); lo.setf(sys::network_interface::flag::up); }
        std::for_each(
            _arguments.begin(), _arguments.end(),
            [this] (const sys::argstream& rhs) {
                _procs.emplace([&rhs] () {
                    return sys::this_process::execute(rhs.argv());
                });
            }
        );
    }

    int
    wait() {
        int retval = 0;
        if (_strategy == Strategy::Peer_to_peer) {
            retval = accumulate_return_value(_procs.begin(), _procs.end());
        } else if (_strategy == Strategy::Master_slave) {
            // wait for master process
            if (_procs.front()) {
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
        out << "Strategy: " << rhs._strategy << '\n';
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

    Strategy _strategy = Strategy::Peer_to_peer;
    std::vector<sys::argstream> _arguments;
    sys::process_group _procs;

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
