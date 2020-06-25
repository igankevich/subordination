#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <regex>
#include <string>
#include <thread>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/fs/path>
#include <unistdx/io/pipe>
#include <unistdx/io/poller>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>
#include <unistdx/net/veth_interface>

#include <gtest/gtest.h>

#include <dtest/application.hh>

#include <subordination/daemon/discovery_test.hh>
#include <subordination/daemon/test_application.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("tst", args...);
}

sys::argstream sbnd_args() {
    sys::argstream args;
    args.append(SBND_PATH);
    args.append("fanout=2");
    args.append("allow-root=1");
    // tolerate asynchronous start of daemons
    args.append("connection-timeout=1s");
    args.append("max-connection-attempts=10");
    args.append("network-scan-interval=5s");
    // never update NICs
    args.append("network-interface-update-interval=1h");
    return args;
}

int main(int argc, char* argv[]) {
    dts::application app;
    dts::cluster cluster;
    cluster.name("x");
    cluster.network({{10,1,0,1},16});
    cluster.peer_network({{10,0,0,1},16});
    cluster.generate_nodes(2);
    app.exit_code(dts::exit_code::all);
    //app.execution_delay(std::chrono::seconds(1));
    app.cluster(std::move(cluster));
    {
        // run sbnd on each cluster node
        const auto num_nodes = app.cluster().size();
        for (size_t i=0; i<num_nodes; ++i) {
            auto args = sbnd_args();
            std::stringstream tmp;
            tmp << app.cluster().name() << (i+1);
            const auto& transactions_directory = tmp.str();
            std::remove(sys::path(transactions_directory, "transactions").data());
            args << "transactions-directory=" << transactions_directory << '\0';
            app.add_process(i, std::move(args));
        }
    }
    app.emplace_test(
        "Wait for cluster to start.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event_sequence(lines, {
                R"(^x1.*add interface address 10\.0\.0\.1.*$)",
                R"(^x1.*add subordinate 10\.0\.0\.2.*$)",
            });
            dts::expect_event(lines, R"(^x1.*set 10\.0\.0\.2.*weight to 1$)");
            dts::expect_event_sequence(lines, {
                R"(^x2.*add interface address 10\.0\.0\.2.*$)",
                R"(^x2.*set principal to 10\.0\.0\.1.*$)"
            });
            dts::expect_event(lines, R"(^x2.*set 10\.0\.0\.1.*weight to 1$)");
        });
    app.emplace_test(
        "Run test application.",
        [&] (dts::application& app, const dts::string_array& lines) {
            sys::argstream args;
            args.append(SBNC_PATH);
            args.append("-T");
            args.append(APP_PATH);
            args.append("transactions");
            // submit test application from the first node
            app.run_process(dts::cluster_node_bitmap(app.cluster().size(), {0}),
                            std::move(args));
        });
    app.emplace_test(
        "Check that at least one subordinate kernel is executed on each node.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event(lines, R"(^x1.*app.*subordinate act.*$)");
            dts::expect_event(lines, R"(^x2.*app.*subordinate act.*$)");
        });
    app.emplace_test(
        "Wait for transaction test to exit.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event(lines, R"(^.*sbnc.*exit code.*$)");
        });
    return dts::run(app);
}
