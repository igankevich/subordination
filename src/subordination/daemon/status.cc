#include <subordination/config.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/small_factory.hh>
#include <subordination/daemon/status_kernel.hh>

using namespace sbnd;

template <class T> void
rec(std::ostream& out, const char* key, const T& value) {
    out << key << ": " << value << '\n';
}

void write_rec(std::ostream& out, const Status_kernel::hierarchy_type& h) {
    rec(out, "interface-address", h.interface_address());
    rec(out, "principal", h.principal());
    out << "subordinates: ";
    if (!h.subordinates().empty()) {
        auto first = h.subordinates().begin();
        auto last = h.subordinates().end();
        out << *first;
        while (first != last) {
            out << ' ' << *first;
            ++first;
        }
    }
    out << '\n';
}


class Main: public sbn::kernel {

private:
    int _argc;
    char** _argv;

public:
    inline explicit Main(int argc, char** argv): _argc(argc), _argv(argv) {}

    void act() override {
        auto* status = new Status_kernel;
        status->destination(sys::socket_address(SUBORDINATION_UNIX_DOMAIN_SOCKET));
        status->set_principal_id(1); // TODO
        status->parent(this);
        sbnc::factory.remote().send(status);
    }

    void
    react(sbn::kernel* child) override {
        auto* status = dynamic_cast<Status_kernel*>(child);
        if (status->return_code() != sbn::exit_code::success) {
            sys::log_message("status", "error: _", status->return_code());
        } else {
            for (const auto& h : status->hierarchies()) {
                write_rec(std::cout, h);
            }
        }
        sbn::graceful_shutdown(int(status->return_code()));
    }

};

int main(int argc, char* argv[]) {
    //sbn::install_error_handler();
    sbnc::factory.types().add<Status_kernel>(4);
    try {
        sbnc::factory.start();
        sbnc::factory.remote().use_localhost(false);
        sbnc::factory.local().send(new Main(argc, argv));
    } catch (const std::exception& err) {
        sys::log_message("submit", "failed to connect to daemon process: _", err.what());
        sbn::graceful_shutdown(1);
    }
    auto ret = sbn::wait_and_return();
    sbnc::factory.stop();
    sbnc::factory.wait();
    return ret;
}
