#include <iostream>

#include <unistdx/base/command_line>
#include <unistdx/fs/canonical_path>
#include <unistdx/fs/mkdirs>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/system/error>

#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/properties.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/factory.hh>
#include <subordination/daemon/job_status_kernel.hh>
#include <subordination/daemon/main.hh>
#include <subordination/daemon/main_kernel.hh>
#include <subordination/daemon/pipeline_status_kernel.hh>
#include <subordination/daemon/status_kernel.hh>
#include <subordination/daemon/terminate_kernel.hh>
#include <subordination/daemon/transaction_test_kernel.hh>

#if defined(SBND_WITH_GLUSTERFS)
#include <subordination/daemon/glusterfs/glusterfs_kernel.hh>
#endif

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("sbnd", args...);
}

void on_terminate(int) {
    // TODO flush transactions
    sbn::exit(0);
}

void print_pipeline_state(int sig) {
    using namespace sbnd;
    {
        std::stringstream tmp;
        factory.write(tmp);
        log("pipeline state _", tmp.str());
        {
            auto g = factory.process().guard();
            for (const auto& pair : factory.process().jobs()) {
                const auto& connection = pair.second;
                log("kill _", connection->child_process_id());
                sys::process_view(connection->child_process_id()).send(sys::signal(sig));
            }
        }
    }
    std::_Exit(sig);
}

void parse_standard_arguments(int argc, char** argv) {
    if (argc != 2) { return; }
    std::string h("-h"), help("--help"), version("--version");
    if (argv[1] == h || argv[1] == help) {
        std::cout << argv[0] << " [-h] [--help] [--version] [key=value]...";
        std::exit(0);
    } else if (argv[1] == version) {
        std::cout << 1;
        std::exit(0);
    }
}

int real_main(int argc, char* argv[]) {
    using namespace sbnd;
    using sbn::Duration;
    parse_standard_arguments(argc, argv);
    sbn::install_error_handler();
    sys::this_process::bind_signal(sys::signal::terminate, on_terminate);
    sys::this_process::bind_signal(sys::signal::keyboard_interrupt, on_terminate);
    sys::this_process::bind_signal(sys::signal::quit, print_pipeline_state);
    factory.types().add<Foreign_main_kernel>(1);
    factory.types().add<probe>(2);
    factory.types().add<Hierarchy_kernel>(3);
    factory.types().add<Status_kernel>(4);
    factory.types().add<Terminate_kernel>(5);
    factory.types().add<Transaction_test_kernel>(6);
    factory.types().add<Transaction_gather_subordinate>(7);
    factory.types().add<Job_status_kernel>(8);
    factory.types().add<Pipeline_status_kernel>(9);
    Properties props;
    props.read(argc, argv);
    if (props.discover.profile) {
        using namespace std::chrono;
        const auto now = system_clock::now().time_since_epoch();
        const auto t = duration_cast<microseconds>(now);
        sys::log_message("profile-node-discovery", "`((time . _))", t.count());
    }
    factory.configure(props);
    try {
        if (factory.isset(factory_flags::unix)) {
            factory.unix().add_server(sys::unix_socket_address(SBND_SOCKET));
        }
        {
            auto k = sbn::make_pointer<Main>(props);
            k->id(1);
            factory.instances().add(k.get());
            if (factory.isset(factory_flags::process)) {
                factory.process().add_listener(k.get());
            }
            if (factory.isset(factory_flags::remote)) {
                factory.remote().add_listener(k.get());
            }
            factory.local().send(std::move(k));
        }
        #if defined(SBND_WITH_GLUSTERFS)
        {
            auto k = sbn::make_pointer<glusterfs_kernel>(props.glusterfs);
            k->id(2);
            factory.instances().add(k.get());
            if (factory.isset(factory_flags::remote)) {
                auto& remote = factory.remote();
                remote.add_listener(k.get());
                remote.scheduler().add_file_system(k->file_system());
            }
            factory.local().send(std::move(k));
        }
        #endif
        factory.start();
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

int main(int argc, char* argv[]) {
    try {
        return real_main(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
