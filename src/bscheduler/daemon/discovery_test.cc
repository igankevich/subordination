#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

#include <unistdx/base/log_message>
#include <unistdx/fs/mkdirs>
#include <unistdx/fs/path>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>

#include <bscheduler/config.hh>

#include <gtest/gtest.h>

#include "test_application.hh"

std::string
cluster_name() {
	return std::getenv("_NAME");
}

std::string
failure() {
	return std::getenv("_FAILURE");
}

int _num_nodes = 0;
int _fanout = 1000;

int
num_nodes() {
	return _num_nodes;
}

sys::path
get_process_output_filename(int n) {
	std::stringstream filename;
	filename << cluster_name() << n << ".out";
	return sys::path(std::getenv("_LOGDIR"), filename.str());
}

void
expect_event_sequence(
	const std::string& filename,
	const std::vector<std::string>& regex_strings
) {
	std::ifstream in(filename);
	EXPECT_TRUE(in.is_open()) << "filename=" << filename;
	std::vector<std::regex> expressions;
	for (const std::string& l : regex_strings) {
		expressions.emplace_back(l);
	}
	auto first = expressions.begin();
	auto last = expressions.end();
	std::string line;
	while (std::getline(in, line) && first != last) {
		if (std::regex_match(line, *first)) {
			++first;
		}
	}
	if (first != last) {
		std::stringstream msg;
		msg << "unmatched expressions: ";
		size_t offset = first - expressions.begin();
		std::copy(
			regex_strings.begin() + offset,
			regex_strings.end(),
			std::ostream_iterator<std::string>(msg, "\n")
		);
		FAIL() << msg.str();
	} else {
		SUCCEED();
	}
}

void
expect_event_count(
	const std::string& filename,
	const std::string& regex_string,
	size_t expected_count
) {
	std::ifstream in(filename);
	EXPECT_TRUE(in.is_open()) << "filename=" << filename;
	std::regex expr(regex_string);
	std::string line;
	size_t count = 0;
	while (std::getline(in, line)) {
		if (std::regex_match(line, expr)) {
			++count;
		}
	}
	EXPECT_EQ(expected_count, count);
}

void
expect_event_sequence(int n, const std::vector<std::string>& regex_strings) {
	expect_event_sequence(get_process_output_filename(n), regex_strings);
}

void
expect_event(int n, const std::string& regex_string) {
	expect_event_sequence(get_process_output_filename(n), {regex_string});
}

void
expect_event_count(int n, const std::string& regex_string, size_t count) {
	expect_event_count(
		get_process_output_filename(n),
		regex_string,
		count
	);
}

TEST(Discovery, Daemon) {
	const int n = num_nodes();
	if (_fanout >= n || n == 2) {
		if (n == 2) {
			expect_event_sequence(1, {
				R"(^.*add ifaddr 10\.0\.0\.1.*$)",
				R"(^.*add subordinate 10\.0\.0\.2.*$)",
			});
			expect_event(1, R"(^.*set 10\.0\.0\.2.*weight to 1$)");
		} else {
			expect_event(1, R"(^.*add ifaddr 10\.0\.0\.1.*$)");
			for (int i=2; i<=n; ++i) {
				std::stringstream re;
				re <<  R"(^.*add subordinate 10\.0\.0\.)" << i << ".*$";
				expect_event(1, re.str());
			}
			for (int i=2; i<=n; ++i) {
				std::stringstream re;
				re << R"(^.*set 10\.0\.0\.)" << i << ".*weight to 1$";
				expect_event(1, re.str());
			}
		}
	}
}

TEST(Discovery, Slaves) {
	const int n = num_nodes();
	if (_fanout >= n || n == 2) {
		if (n == 2) {
			expect_event_sequence(2, {
				R"(^.*add ifaddr 10\.0\.0\.2.*$)",
				R"(^.*set principal to 10\.0\.0\.1.*$)"
			});
			expect_event(2, R"(^.*set 10\.0\.0\.1.*weight to 1$)");
		} else {
			for (int i=2; i<=n; ++i) {
				std::stringstream re;
				re << R"(^.*add ifaddr 10\.0\.0\.)" << i << ".*$";
				expect_event(i, re.str());
				expect_event(i,	R"(^.*set principal to 10\.0\.0\.1.*$)");
				std::stringstream re2;
				re2 << R"(^.*set 10\.0\.0\.1.*weight to )" << n-1 << "$";
				expect_event(i, re2.str());
			}
		}
	}
}

TEST(Discovery, Fanout2Hierarchy) {
	const int n = num_nodes();
	if (_fanout != 2 || n == 2) {
		return;
	}
	expect_event(1, R"(^.*add ifaddr 10\.0\.0\.1.*$)");
	if (n == 4) {
		expect_event(1, R"(^.*add subordinate 10\.0\.0\.2.*$)");
		expect_event(1, R"(^.*add subordinate 10\.0\.0\.3.*$)");
		expect_event(2, R"(^.*add subordinate 10\.0\.0\.4.*$)");
	} else if (n == 8) {
		expect_event(1, R"(^.*add subordinate 10\.0\.0\.2.*$)");
		expect_event(1, R"(^.*add subordinate 10\.0\.0\.3.*$)");
		expect_event(2, R"(^.*add subordinate 10\.0\.0\.4.*$)");
		expect_event(2, R"(^.*add subordinate 10\.0\.0\.5.*$)");
		expect_event(3, R"(^.*add subordinate 10\.0\.0\.6.*$)");
		expect_event(3, R"(^.*add subordinate 10\.0\.0\.7.*$)");
		expect_event(4, R"(^.*add subordinate 10\.0\.0\.8.*$)");
	} else {
		FAIL() << "bad number of nodes: " << n;
	}
}

TEST(Discovery, Fanout2Weights) {
	const int n = num_nodes();
	if (_fanout != 2 || n == 2) {
		return;
	}
	expect_event(1, R"(^.*add ifaddr 10\.0\.0\.1.*$)");
	if (n == 4) {
		// upstream
		expect_event(1, R"(^.*set 10\.0\.0\.2.*weight to 2$)");
		expect_event(1, R"(^.*set 10\.0\.0\.3.*weight to 1$)");
		expect_event(2, R"(^.*set 10\.0\.0\.4.*weight to 1$)");
		// downstream
		expect_event(2, R"(^.*set 10\.0\.0\.1.*weight to 2$)");
		expect_event(3, R"(^.*set 10\.0\.0\.1.*weight to 3$)");
		expect_event(4, R"(^.*set 10\.0\.0\.2.*weight to 3$)");
	} else if (n == 8) {
		// upstream
		expect_event(1, R"(^.*set 10\.0\.0\.2.*weight to 4$)");
		expect_event(1, R"(^.*set 10\.0\.0\.3.*weight to 3$)");
		expect_event(2, R"(^.*set 10\.0\.0\.4.*weight to 2$)");
		expect_event(2, R"(^.*set 10\.0\.0\.5.*weight to 1$)");
		expect_event(3, R"(^.*set 10\.0\.0\.6.*weight to 1$)");
		expect_event(3, R"(^.*set 10\.0\.0\.7.*weight to 1$)");
		expect_event(4, R"(^.*set 10\.0\.0\.8.*weight to 1$)");
		// downstream
		expect_event(2, R"(^.*set 10\.0\.0\.1.*weight to 4$)");
		expect_event(3, R"(^.*set 10\.0\.0\.1.*weight to 5$)");
		expect_event(4, R"(^.*set 10\.0\.0\.2.*weight to 6$)");
		expect_event(5, R"(^.*set 10\.0\.0\.2.*weight to 7$)");
		expect_event(6, R"(^.*set 10\.0\.0\.3.*weight to 7$)");
		expect_event(7, R"(^.*set 10\.0\.0\.3.*weight to 7$)");
		expect_event(8, R"(^.*set 10\.0\.0\.4.*weight to 7$)");
	} else {
		FAIL() << "bad number of nodes: " << n;
	}
}

TEST(Discovery, TestApplication) {
	if (std::getenv("_SUBMIT")) {
		int process_no = failure() == "master-failure" ? 2 : 1;
		expect_event(
			process_no,
			R"(^.*app exited:.*status=exited,exit_code=0.*$)"
		);
	}
}

int main(int argc, char* argv[]) {
	sys::mkdirs(sys::path("."), sys::path(BSCHEDULER_LOG_DIRECTORY "/"));
	_num_nodes = std::atoi(std::getenv("_NODES"));
	_fanout = std::atoi(std::getenv("_FANOUT"));
	std::string s = "1-";
	s += std::getenv("_NODES");
	sys::argstream args;
	args.append("unshare");
	args.append("--map-root-user");
	args.append("--net");
	args.append("--mount");
	args.append(std::getenv("_CLUSTER"));
	args.append("--name");
	args.append(cluster_name());
	args.append("--nodes");
	args.append(s);
	args.append("--outdir");
	args.append(std::getenv("_LOGDIR"));
	if (const char* submit = std::getenv("_SUBMIT")) {
		args.append("--submit");
		args.append(submit);
	}
	args.append("--");
	for (int i=1; i<argc; ++i) {
		args.append(argv[i]);
	}
	sys::log_message("tst", "executing _", args);
	sys::process child([&] () {
		try {
			return sys::this_process::execute_command(args);
		} catch (const std::system_error& err) {
			sys::log_message("tst", "failed to execute unshare: _", err.what());
			return 1;
		}
	});
	sys::proc_status status = child.wait();
	if (status.exited() && status.exit_code() == 0) {
		int no_argc = 0;
		char** no_argv = nullptr;
		::testing::InitGoogleTest(&no_argc, no_argv);
		return RUN_ALL_TESTS();
	}
	return status.exit_code();
}
