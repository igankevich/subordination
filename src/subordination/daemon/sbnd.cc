#include <iostream>

#include <unistdx/base/command_line>
#include <unistdx/fs/canonical_path>
#include <unistdx/fs/mkdirs>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/network_master.hh>
#include <subordination/daemon/status_kernel.hh>
#include <subordination/daemon/terminate_kernel.hh>

class Duration: public std::chrono::system_clock::duration {
public:
    using base_duration = std::chrono::system_clock::duration;
    using base_duration::duration;
};

inline void trim_right(std::string& s) {
    while (!s.empty() && s.back() <= ' ') { s.pop_back(); }
}

inline void trim(std::string& s) {
    trim_right(s);
    std::string::size_type i = 0, n = s.size();
    for (; i<n && s[i] <= ' '; ++i);
    if (i != 0) { s = s.substr(i); }
}

Duration string_to_duration(std::string s) {
    using namespace std::chrono;
    using d = Duration::base_duration;
    using days = std::chrono::duration<Duration::rep,std::ratio<60*60*24>>;
    trim_right(s);
    std::size_t i = 0, n = s.size();
    if (i == n) { throw std::invalid_argument("duration without suffix"); }
    Duration::rep value = std::stoul(s, &i);
    auto suffix = s.substr(i);
    if (suffix == "ns") { return duration_cast<d>(nanoseconds(value)); }
    if (suffix == "us") { return duration_cast<d>(microseconds(value)); }
    if (suffix == "ms") { return duration_cast<d>(milliseconds(value)); }
    if (suffix == "s") { return duration_cast<d>(seconds(value)); }
    if (suffix == "m") { return duration_cast<d>(minutes(value)); }
    if (suffix == "h") { return duration_cast<d>(hours(value)); }
    if (suffix == "d") { return duration_cast<d>(days(value)); }
    std::stringstream tmp;
    tmp << "unknown duration suffix \"" << suffix << "\"";
    throw std::invalid_argument(tmp.str());
}

std::istream& operator>>(std::istream& in, Duration& rhs) {
    std::string s; in >> s; rhs = string_to_duration(s); return in;
}

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

void on_terminate(int s) { sbn::exit(0); }

int main(int argc, char* argv[]) {
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
    Duration connection_timeout = std::chrono::seconds(7);
    sys::u32 max_connection_attempts = 1;
    Duration network_interface_update_interval = std::chrono::minutes(1);
    Duration network_scan_interval = std::chrono::minutes(1);
    sys::path transactions_directory(SBND_SHARED_STATE_DIR);
    sys::input_operator_type options[] = {
        sys::ignore_first_argument(),
        sys::make_key_value("fanout", fanout),
        sys::make_key_value("servers", servers),
        sys::make_key_value("allow-root", allow_root),
        sys::make_key_value("connection-timeout", connection_timeout),
        sys::make_key_value("max-connection-attempts", max_connection_attempts),
        sys::make_key_value("network-interface-update-interval",
                            network_interface_update_interval),
        sys::make_key_value("network-scan-interval", network_scan_interval),
        sys::make_key_value("transactions-directory", transactions_directory),
        nullptr
    };
    sys::parse_arguments(argc, argv, options);
    sbn::install_error_handler();
    install_debug_handler();
    sys::this_process::bind_signal(sys::signal::terminate, on_terminate);
    sys::this_process::bind_signal(sys::signal::keyboard_interrupt, on_terminate);
    factory.types().add<probe>(2);
    factory.types().add<Hierarchy_kernel>(3);
    factory.types().add<Status_kernel>(4);
    factory.types().add<Terminate_kernel>(5);
    try {
        sys::mkdirs(transactions_directory);
        transactions_directory = sys::canonical_path(transactions_directory);
        factory.transactions(sys::path(transactions_directory, "transactions").data());
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        factory.unix().add_server(sys::socket_address(SBND_SOCKET));
        factory.process().allow_root(allow_root);
        #endif
        auto m = sbn::make_pointer<network_master>();
        m->id(1);
        m->allow(servers);
        m->fanout(fanout);
        m->interval(network_interface_update_interval);
        m->network_scan_interval(network_scan_interval);
        factory.instances().add(m.get());
        factory.process().add_listener(m.get());
        factory.remote().connection_timeout(connection_timeout);
        factory.remote().max_connection_attempts(max_connection_attempts);
        factory.remote().add_listener(m.get());
        factory.start();
        factory.local().send(std::move(m));
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        sbn::exit(1);
    }
    auto ret = sbn::wait_and_return();
    factory.stop();
    factory.wait();
    factory.clear();
    return ret;
}
