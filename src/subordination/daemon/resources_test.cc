#include <sstream>

#include <dtest/application.hh>
#include <subordination/daemon/discovery_test.hh>
#include <valgrind/config.hh>

sys::argstream sbnd_args() {
    sys::argstream args;
    args.append(SBND_PATH);
    args.append("discoverer.fanout=2");
    args.append("process.allow-root=1");
    // tolerate asynchronous start of daemons
    args.append("remote.connection-timeout=1s");
    args.append("remote.max-connection-attempts=10");
    args.append("discoverer.scan-interval=5s");
    // never update NICs
    args.append("network.interface-update-interval=1h");
    // disable transactions
    args.append("factory.flags=-transactions");
    return args;
}

int main(int argc, char* argv[]) {
    SBN_SKIP_IF_RUNNING_ON_VALGRIND();
    dts::cluster cluster;
    cluster.name("x");
    cluster.network({{10,1,0,1},16});
    cluster.peer_network({{10,0,0,1},16});
    cluster.generate_nodes(2);
    dts::application app;
    app.exit_code(dts::exit_code::all);
    app.cluster(std::move(cluster));
    // run sbnd on each node
    const auto num_nodes = app.cluster().size();
    for (size_t i=0; i<num_nodes; ++i) {
        auto args = sbnd_args();
        args << "resources.tag=" << i+1 << '\0';
        app.add_process(i, std::move(args));
    }
    app.emplace_test(
        "Wait for superior node to find subordinate nodes.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event_sequence(lines, {
                R"(^x1.*test.*add interface address 10\.0\.0\.1.*$)",
                R"(^x1.*test.*add subordinate 10\.0\.0\.2.*$)",
            });
        });
    app.emplace_test(
        "Wait for subordinate nodes to find superior node.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event_sequence(lines, {
                R"(^x2.*test.*add interface address 10\.0\.0\.2.*$)",
                R"(^x2.*test.*set principal to 10\.0\.0\.1.*$)"
            });
        });
    app.emplace_test(
        "Run test application.",
        [&] (dts::application& app, const dts::string_array& lines) {
            sys::argstream args;
            args.append(SBNC_PATH);
            args.append("submit");
            args.append(RESOURCES_APP_PATH);
            // submit test application from the first node
            app.run_process(dts::cluster_node_bitmap(app.cluster().size(), {0}),
                            std::move(args));
        });
    app.emplace_test(
        "Check that all the kernels are executed on x2.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event_count(lines, R"(^x2.*app.*subordinate hostname x2.*$)", 10);
        });
    app.emplace_test(
        "Check that test application finished successfully.",
        [] (dts::application& app, const dts::string_array& lines) {
            dts::expect_event(lines, R"(^x1.*test.*job .* terminated with status .*$)");
        });
    return dts::run(app);
}
