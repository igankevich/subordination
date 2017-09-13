#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

#include <unistdx/base/log_message>
#include <unistdx/fs/path>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>

#include <gtest/gtest.h>

const std::string cluster_name = "cat";

sys::path
get_process_output_filename(int n) {
	std::stringstream filename;
	filename << cluster_name << n << ".out";
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
expect_event_sequence(int n, const std::vector<std::string>& regex_strings) {
	expect_event_sequence(get_process_output_filename(n), regex_strings);
}

TEST(Discovery, Daemon) {
	expect_event_sequence(1, {
		R"(^.*add ifaddr 10\.0\.0\.1.*$)",
		R"(^.*add subordinate 10\.0\.0\.2.*$)"
	});
}

TEST(Discovery, Slave) {
	expect_event_sequence(2, {
		R"(^.*add ifaddr 10\.0\.0\.2.*$)",
		R"(^.*set principal to 10\.0\.0\.1.*$)",
		R"(^.*unset principal 10\.0\.0\.1.*$)"
	});
}

int main(int argc, char* argv[]) {
	sys::argstream args;
	args.append("unshare");
	args.append("--map-root-user");
	args.append("--net");
	args.append("--mount");
	args.append(std::getenv("_CLUSTER"));
	args.append("--name");
	args.append(cluster_name);
	args.append("--nodes");
	args.append("1-2");
	args.append("--outdir");
	args.append(std::getenv("_LOGDIR"));
	args.append("--");
	for (int i=1; i<argc; ++i) {
		args.append(argv[i]);
	}
	sys::log_message("tst", "executing _", args);
	sys::process child([&] () {
		try {
			return sys::this_process::exec_command(args.argv());
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
