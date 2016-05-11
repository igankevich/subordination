#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <iomanip>

#include <stdx/iterator.hh>
#include <sys/process.hh>
#include <sys/cmdline.hh>

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

int main(int argc, char* argv[]) {
	Strategy strat = Strategy::Peer_to_peer;
	std::vector<sys::argstream> arguments;
	sys::cmdline cmdline(argc, argv, {
		sys::cmd::ignore_first_arg(),
		sys::cmd::make_option({"--strategy"}, strat),
		[&arguments] (const sys::cmdline::arg_type& arg, sys::cmdline::pos_type, sys::cmdline::stream_type& str) {
			if (arg == "--exec") {
				arguments.emplace_back();
			} else {
				arguments.back().append(arg);
			}
			return true;
		}
	});
	cmdline.parse();
	std::clog << "Strategy = " << strat << std::endl;
	std::for_each(
		arguments.begin(), arguments.end(),
		[] (const sys::argstream& rhs) {
			std::clog << "argc=" << rhs.argc() << ' ';
			std::copy(
				rhs.argv(), rhs.argv() + rhs.argc(),
				stdx::intersperse_iterator<char*,char>(std::clog, ' ')
			);
			std::clog << std::endl;
		}
	);
	sys::process_group procs;
	std::for_each(
		arguments.begin(), arguments.end(),
		[&procs] (const sys::argstream& rhs) {
			procs.emplace([&rhs] () {
				return sys::this_process::execute(rhs.argv());
			});
		}
	);
	int retval = 0;
	if (strat == Strategy::Peer_to_peer) {
		std::for_each(
			procs.begin(), procs.end(),
			[&retval] (sys::process& rhs) {
				sys::proc_status stat = rhs.wait();
				std::clog << "child process terminated: " << stat << std::endl;
				retval |= stat.exit_code() | sys::signal_type(stat.term_signal());
			}
		);
	} else if (strat == Strategy::Master_slave) {
		// wait for master process
		if (procs.front()) {
			sys::proc_status stat = procs.front().wait();
			retval |= stat.exit_code() | sys::signal_type(stat.term_signal());
		}
		// wait for child processes
		std::for_each(
			procs.begin()+1, procs.end(),
			[&retval] (sys::process& rhs) {
				if (rhs) {
					rhs.terminate();
				}
				sys::proc_status stat = rhs.wait();
				std::clog << "child process terminated: " << stat << std::endl;
				if (stat.term_signal() != sys::signal::terminate) {
					retval |= stat.exit_code() | sys::signal_type(stat.term_signal());
				}
			}
		);
	}
	return retval;
}
