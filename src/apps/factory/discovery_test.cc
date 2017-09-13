#include <cstdlib>
#include <fstream>
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

bool
line_matches(const std::string& filename, const std::regex& expr) {
	std::ifstream in(filename);
	EXPECT_TRUE(in.is_open()) << "filename=" << filename;
	std::string line;
	bool found = false;
	while (std::getline(in, line) && !found) {
		if (std::regex_match(line, expr)) {
			found = true;
		}
	}
	return found;
}

bool
line_matches(int n, const std::regex& expr) {
	return line_matches(get_process_output_filename(n), expr);
}

#define EXPECT_LINE_MATCH(n, expr) \
	EXPECT_TRUE(line_matches(n, std::regex(expr)))

TEST(Discovery, Daemon) {
	EXPECT_LINE_MATCH(1, R"(^.*add ifaddr 10\.0\.0\.1.*$)");
	EXPECT_LINE_MATCH(1, R"(^.*add subordinate 10\.0\.0\.2.*$)");
}

TEST(Discovery, Slave) {
	EXPECT_LINE_MATCH(2, R"(^.*add ifaddr 10\.0\.0\.2.*$)");
	EXPECT_LINE_MATCH(2, R"(^.*set principal to 10\.0\.0\.1.*$)");
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
