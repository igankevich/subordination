#include <iostream>

#include <unistdx/base/command_line>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/config.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/application_kernel.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/network_master.hh>
#include <subordination/daemon/status_kernel.hh>

void
print_state(int) {
    std::clog << __func__ << std::endl;
    //sbnd::factory.print_state(std::clog);
}

void
install_debug_handler() {
    using namespace sys::this_process;
    bind_signal(sys::signal::quit, print_state);
}

int
main(int argc, char* argv[]) {
    #if defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    {
        using namespace std::chrono;
        const auto now = system_clock::now().time_since_epoch();
        const auto t = duration_cast<milliseconds>(now);
        sys::log_message("discovery", "time since epoch _ms", t.count());
    }
    #endif
    using namespace sbnd;
    sys::ipv4_address::rep_type fanout = 10000;
    sys::interface_address<sys::ipv4_address> servers;
    bool allow_root = false;
    sys::input_operator_type options[] = {
        sys::ignore_first_argument(),
        sys::make_key_value("fanout", fanout),
        sys::make_key_value("servers", servers),
        sys::make_key_value("allow_root", allow_root),
        nullptr
    };
    sys::parse_arguments(argc, argv, options);
    sbn::install_error_handler();
    install_debug_handler();
    sbn::types.register_type<application_kernel>(1);
    sbn::types.register_type<probe>(2);
    sbn::types.register_type<Hierarchy_kernel>(3);
    sbn::types.register_type<Status_kernel>(4);
    try {
        factory.start();
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        factory.external().add_server(sys::socket_address(SUBORDINATION_UNIX_DOMAIN_SOCKET));
        factory.child().allow_root(allow_root);
        #endif
        network_master* m = new network_master;
        m->id(1);
        m->allow(servers);
        m->fanout(fanout);
        {
            sbn::instances_guard g(sbn::instances);
            sbn::instances.add(m);
        }
        factory.local().send(m);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        sbn::graceful_shutdown(1);
    }
    auto ret = sbn::wait_and_return();
    factory.stop();
    factory.wait();
    return ret;
}
