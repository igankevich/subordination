#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <regex>
#include <string>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/fs/path>
#include <unistdx/io/pipe>
#include <unistdx/io/poller>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>

#include <subordination/config.hh>

#include <gtest/gtest.h>

#include <subordination/daemon/test_application.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("tst", args...);
}

int _num_nodes = 0;
int _fanout = 1000;
std::string expected_failure;
std::string cluster_name;

void
expect_event_sequence(
    const std::vector<std::string>& lines,
    const std::vector<std::string>& regex_strings
) {
    std::vector<std::regex> expressions;
    for (const auto& s : regex_strings) { expressions.emplace_back(s); }
    auto first = expressions.begin();
    auto last = expressions.end();
    auto first2 = lines.begin();
    auto last2 = lines.end();
    std::string line;
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

void
expect_event(const std::vector<std::string>& lines, const std::string& regex_string) {
    expect_event_sequence(lines, {regex_string});
}

void
expect_event_count(
    const std::vector<std::string>& lines,
    const std::string& regex_string,
    size_t expected_count
) {
    std::regex expr(regex_string);
    std::string line;
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

void test_master(const std::vector<std::string>& lines, int fanout, int n) {
    if (fanout >= n || n == 2) {
        if (n == 2) {
            expect_event_sequence(lines, {
                R"(^x1.*add interface address 10\.0\.0\.1.*$)",
                R"(^x1.*add subordinate 10\.0\.0\.2.*$)",
            });
            expect_event(lines, R"(^x1.*set 10\.0\.0\.2.*weight to 1$)");
        } else {
            expect_event(lines, R"(^x1.*add interface address 10\.0\.0\.1.*$)");
            for (int i=2; i<=n; ++i) {
                std::stringstream re;
                re << R"(^x1.*add subordinate 10\.0\.0\.)" << i << ".*$";
                expect_event(lines, re.str());
            }
            for (int i=2; i<=n; ++i) {
                std::stringstream re;
                re << R"(^x1.*set 10\.0\.0\.)" << i << ".*weight to 1$";
                expect_event(lines, re.str());
            }
        }
    }
}

void test_slaves(const std::vector<std::string>& lines, int fanout, int n) {
    if (fanout >= n || n == 2) {
        if (n == 2) {
            expect_event_sequence(lines, {
                R"(^x2.*add interface address 10\.0\.0\.2.*$)",
                R"(^x2.*set principal to 10\.0\.0\.1.*$)"
            });
            expect_event(lines, R"(^x2.*set 10\.0\.0\.1.*weight to 1$)");
        } else {
            for (int i=2; i<=n; ++i) {
                std::stringstream re;
                re << "^x" << i << R"(.*add interface address 10\.0\.0\.)" << i << ".*$";
                expect_event(lines, re.str());
                re.str("");
                re << "^x" << i << R"(.*set principal to 10\.0\.0\.1.*$)";
                expect_event(lines,	re.str());
                re.str("");
                re << "^x" << i << R"(.*set 10\.0\.0\.1.*weight to )" << n-1 << "$";
                expect_event(lines, re.str());
            }
        }
    }
}

void test_hierarchy(const std::vector<std::string>& lines, int fanout, int n) {
    if (fanout != 2 || n == 2) { return; }
    expect_event(lines, R"(^x1.*add interface address 10\.0\.0\.1.*$)");
    if (n == 4) {
        expect_event(lines, R"(^x1.*add subordinate 10\.0\.0\.2.*$)");
        expect_event(lines, R"(^x1.*add subordinate 10\.0\.0\.3.*$)");
        expect_event(lines, R"(^x2.*add subordinate 10\.0\.0\.4.*$)");
    } else if (n == 8) {
        expect_event(lines, R"(^x1.*add subordinate 10\.0\.0\.2.*$)");
        expect_event(lines, R"(^x1.*add subordinate 10\.0\.0\.3.*$)");
        expect_event(lines, R"(^x2.*add subordinate 10\.0\.0\.4.*$)");
        expect_event(lines, R"(^x2.*add subordinate 10\.0\.0\.5.*$)");
        expect_event(lines, R"(^x3.*add subordinate 10\.0\.0\.6.*$)");
        expect_event(lines, R"(^x3.*add subordinate 10\.0\.0\.7.*$)");
        expect_event(lines, R"(^x4.*add subordinate 10\.0\.0\.8.*$)");
    } else {
        std::stringstream msg;
        msg << "bad number of nodes: " << n;
        throw std::invalid_argument(msg.str());
    }
}

void test_weights(const std::vector<std::string>& lines, int fanout, int n) {
    if (fanout != 2 || n == 2) { return; }
    expect_event(lines, R"(^x1.*add interface address 10\.0\.0\.1.*$)");
    if (n == 4) {
        // upstream
        expect_event(lines, R"(^x1.*set 10\.0\.0\.2.*weight to 2$)");
        expect_event(lines, R"(^x1.*set 10\.0\.0\.3.*weight to 1$)");
        expect_event(lines, R"(^x2.*set 10\.0\.0\.4.*weight to 1$)");
        // downstream
        expect_event(lines, R"(^x2.*set 10\.0\.0\.1.*weight to 2$)");
        expect_event(lines, R"(^x3.*set 10\.0\.0\.1.*weight to 3$)");
        expect_event(lines, R"(^x4.*set 10\.0\.0\.2.*weight to 3$)");
    } else if (n == 8) {
        // upstream
        expect_event(lines, R"(^x1.*set 10\.0\.0\.2.*weight to 4$)");
        expect_event(lines, R"(^x1.*set 10\.0\.0\.3.*weight to 3$)");
        expect_event(lines, R"(^x2.*set 10\.0\.0\.4.*weight to 2$)");
        expect_event(lines, R"(^x2.*set 10\.0\.0\.5.*weight to 1$)");
        expect_event(lines, R"(^x3.*set 10\.0\.0\.6.*weight to 1$)");
        expect_event(lines, R"(^x3.*set 10\.0\.0\.7.*weight to 1$)");
        expect_event(lines, R"(^x4.*set 10\.0\.0\.8.*weight to 1$)");
        // downstream
        expect_event(lines, R"(^x2.*set 10\.0\.0\.1.*weight to 4$)");
        expect_event(lines, R"(^x3.*set 10\.0\.0\.1.*weight to 5$)");
        expect_event(lines, R"(^x4.*set 10\.0\.0\.2.*weight to 6$)");
        expect_event(lines, R"(^x5.*set 10\.0\.0\.2.*weight to 7$)");
        expect_event(lines, R"(^x6.*set 10\.0\.0\.3.*weight to 7$)");
        expect_event(lines, R"(^x7.*set 10\.0\.0\.3.*weight to 7$)");
        expect_event(lines, R"(^x8.*set 10\.0\.0\.4.*weight to 7$)");
    } else {
        std::stringstream msg;
        msg << "bad number of nodes: " << n;
        throw std::invalid_argument(msg.str());
    }
}

void test_application(const std::vector<std::string>& lines, int fanout, int n) {
    int node_no = 0;
    if (expected_failure == "no-failure") {
        node_no = 1;
    } else if (expected_failure == "master-failure" && n == 2) {
        node_no = 2;
    }
    if (node_no == 0) {
        expect_event(lines, R"(^x.*app exited:.*status=exited,exit_code=0.*$)");
    } else {
        int node_no = (expected_failure == "master-failure") ? 2 : 1;
        std::stringstream re;
        re << "^x" << node_no << R"(.*app exited:.*status=exited,exit_code=0.*$)";
        expect_event(lines, re.str());
    }
}

using s = sys::signal;
sys::process child;

void on_terminate(int sig) { child.terminate(); ::alarm(3); }
void on_alarm(int) { child.kill(); }

void
init_signal_handlers() {
    using namespace sys::this_process;
    ignore_signal(s::broken_pipe);
    bind_signal(s::keyboard_interrupt, on_terminate);
    bind_signal(s::terminate, on_terminate);
    bind_signal(s::alarm, on_alarm);
}

int main(int argc, char* argv[]) {
    auto dtest_exe = std::getenv("DTEST_EXE");
    auto dtest_name = std::getenv("DTEST_NAME");
    auto dtest_size = std::getenv("DTEST_SIZE");
    auto sbn_fanout = std::getenv("SBN_FANOUT");
    auto sbn_failure = std::getenv("SBN_FAILURE");
    if (!dtest_exe) { throw std::invalid_argument("DTEST_EXE is undefined"); }
    if (!dtest_name) { throw std::invalid_argument("DTEST_NAME is undefined"); }
    if (!dtest_size) { throw std::invalid_argument("DTEST_SIZE is undefined"); }
    if (!sbn_fanout) { throw std::invalid_argument("SBN_FANOUT is undefined"); }
    if (!sbn_failure) { throw std::invalid_argument("SBN_FAILURE is undefined"); }
    _num_nodes = std::atoi(dtest_size);
    _fanout = std::atoi(sbn_fanout);
    expected_failure = sbn_failure;
    cluster_name = dtest_name;
    sys::argstream args;
    args.append(dtest_exe);
    args.append("--name", dtest_name);
    args.append("--size", dtest_size);
    args.append("--network", "10.1.0.0/16");
    args.append("--peer-network", "10.0.0.0/16");
    args.append("--exec-delay", "1000");
    for (int i=1; i<argc; ++i) { args.append(argv[i]); }
    log("executing _", args);
    init_signal_handlers();
    sys::pipe stderr;
    stderr.out().unsetf(sys::open_flag::non_blocking);
    child = sys::process{[&args,&stderr] () {
        try {
            stderr.in().close();
            sys::fildes err(STDERR_FILENO);
            err = stderr.out();
            sys::this_process::execute(args);
        } catch (const std::exception& err) {
            log("failed to execute _: _", args, err.what());
        }
        return 1;
    }};
    stderr.out().close();
    sys::event_poller poller;
    poller.emplace(stderr.in().fd(), sys::event::in);
    struct No_lock { void lock() {} void unlock() {} };
    No_lock lock;
    sys::byte_buffer buf(4096);
    std::vector<std::string> all_lines;
    std::vector<std::function<void()>> tests{
        [&] () { test_master(all_lines, _fanout, _num_nodes); },
        [&] () { test_slaves(all_lines, _fanout, _num_nodes); },
        [&] () { test_hierarchy(all_lines, _fanout, _num_nodes); },
        [&] () { test_weights(all_lines, _fanout, _num_nodes); },
        [&] () { test_application(all_lines, _fanout, _num_nodes); },
    };
    auto ntests = tests.size();
    std::vector<std::string> errors(ntests);
    bool success = false;
    poller.wait(lock, [&] () {
        auto pipe_fd = poller.pipe_in();
        for (const auto& event : poller) {
            if (event.fd() == pipe_fd) { continue; }
            buf.fill(stderr.in());
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
                    all_lines.emplace_back(buf.data()+buf.position(), buf.data()+buf.limit()-1);
                    sys::fd_type out(2);
                    buf.flush(out);
                }
                ++first;
            }
            buf.limit(old_limit);
            buf.compact();
            std::clog << std::flush;
        }
        success = true;
        for (size_t i=0; i<ntests; ++i) {
            try {
                errors[i].clear();
                tests[i]();
            } catch (const std::exception& err) {
                success = false;
                errors[i] = err.what();
            }
        }
        for (size_t i=0; i<ntests; ++i) {
            std::cerr << "Test #" << (i+1);
            if (errors[i].empty()) {
                std::cerr << " OK\n";
            } else {
                std::cerr << " FAIL\n" << errors[i] << '\n';
            }
        }
        return success;
    });
    child.terminate();
    auto status = child.wait();
    log("dtest status _ ", status);
    return 0;
}
