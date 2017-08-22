#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <iomanip>
#include <numeric>

#include <unistdx/base/cmdline>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/process>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process_group>
#include <unistdx/it/intersperse_iterator>

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

	Executor(int argc, char* argv[]) {
		sys::input_operator_type options[] = {
			sys::ignore_first_argument(),
			sys::make_key_value("--strategy", strat),
			[this] (int pos, const std::string& arg) {
				if (arg == "--exec") {
					this->arguments.emplace_back();
				} else {
					this->arguments.back().append(arg);
				}
				return true;
			},
			nullptr
		};
		sys::parse_arguments(argc, argv, options);
		assert(!arguments.empty());
	}

	void
	execute() {
		std::for_each(
			arguments.begin(), arguments.end(),
			[this] (const sys::argstream& rhs) {
				procs.emplace([&rhs] () {
					return sys::this_process::execute(rhs.argv());
				});
			}
		);
	}

	int
	wait() {
		int retval = 0;
		if (strat == Strategy::Peer_to_peer) {
			retval = accumulate_return_value(procs.begin(), procs.end());
		} else if (strat == Strategy::Master_slave) {
			// wait for master process
			if (procs.front()) {
				sys::proc_status stat = procs.front().wait();
				std::clog << "master process terminated: " << stat << std::endl;
				retval = stat.exit_code() | sys::signal_type(stat.term_signal());
			}
			// terminate child processes
			if (procs.size() > 1) {
				std::for_each(
					procs.begin()+1, procs.end(),
					[&retval] (sys::process& rhs) {
						if (rhs) {
							rhs.terminate();
						}
						sys::proc_status stat = rhs.wait();
						std::clog << "child process terminated: " << stat << std::endl;
					}
				);
			}
		}
		return retval;
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Executor& rhs) {
		out << "Strategy = " << rhs.strat << '\n';
		std::for_each(
			rhs.arguments.begin(), rhs.arguments.end(),
			[&out] (const sys::argstream& rhs) {
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
				sys::proc_status stat = rhs.wait();
				std::clog << "child process terminated: " << stat << std::endl;
				return ret | stat.exit_code() | sys::signal_type(stat.term_signal());
			}
		);
	}

	Strategy strat = Strategy::Peer_to_peer;
	std::vector<sys::argstream> arguments;
	sys::process_group procs;

};

int main(int argc, char* argv[]) {
	sys::this_process::ignore_signal(sys::signal::broken_pipe);
	Executor exe(argc, argv);
	std::clog << exe << std::flush;
	exe.execute();
	return exe.wait();
}
