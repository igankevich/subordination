#include <iostream>
#include <string>
#include <vector>

#include <unistdx/fs/canonical_path>

#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/small_factory.hh>
#include <subordination/daemon/status_kernel.hh>

enum class Command { Submit, Status };

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("sbnc", args ...);
}

template <class T> void
rec(std::ostream& out, const char* key, const T& value) {
    out << key << ": " << value << '\n';
}

void write_rec(std::ostream& out, const sbnd::Status_kernel::hierarchy_type& h) {
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

class Main_kernel: public sbn::kernel {};


sbn::kernel*
new_application_kernel(int argc, char* argv[]) {
    using string_array = sbn::application::string_array;
    string_array args, env;
    for (int i=1; i<argc; ++i) {
        args.emplace_back(argv[i]);
    }
    for (char** first=environ; *first; ++first) {
        env.emplace_back(*first);
    }
    auto* app = new sbn::application(args, env);
    app->working_directory(sys::canonical_path("."));
    auto* k = new Main_kernel;
    k->target_application(app);
    //k->source_application_id(app->id());
    k->destination(sys::socket_address(SBND_SOCKET));
    return k;
}

class Submit: public sbn::kernel {

private:
    int _argc;
    char** _argv;

public:
    Submit(int argc, char** argv):
    _argc(argc),
    _argv(argv)
    {}

    void act() override {
        auto* k = new_application_kernel(this->_argc, this->_argv);
        k->parent(this);
        sbnc::factory.remote().send(k);
    }

    void react(kernel* child) override {
        auto* k = dynamic_cast<Main_kernel*>(child);
        if (k->return_code() != sbn::exit_code::success) {
            message("failed to submit _", k->target_application_id());
        } else {
            message("submitted _", k->target_application_id());
        }
        sbn::graceful_shutdown(int(k->return_code()));
    }

};

class Status: public sbn::kernel {

    void act() override {
        auto* status = new sbnd::Status_kernel;
        status->destination(sys::socket_address(SBND_SOCKET));
        status->principal_id(1); // TODO
        status->parent(this);
        sbnc::factory.remote().send(status);
    }

    void
    react(sbn::kernel* child) override {
        auto* status = dynamic_cast<sbnd::Status_kernel*>(child);
        if (status->return_code() != sbn::exit_code::success) {
            message("error: _", status->return_code());
        } else {
            for (const auto& h : status->hierarchies()) {
                write_rec(std::cout, h);
            }
        }
        sbn::graceful_shutdown(int(status->return_code()));
    }

};

void usage(char* argv[0]) {
    std::cerr << "usage: " << argv[0] << " [-s] [command] [arguments...]\n";
}

int main(int argc, char* argv[]) {
    Command command = Command::Submit;
    using traits_type = std::char_traits<char>;
    if (argc <= 1) { usage(argv); return 1; }
    if (argc >= 2 && traits_type::compare(argv[1], "-s", 2) == 0) {
        command = Command::Status;
    }
    sbn::install_error_handler();
    sbnc::factory.types().add<Main_kernel>(1);
    sbnc::factory.types().add<sbnd::Status_kernel>(4);
    try {
        sbnc::factory.start();
        sbnc::factory.remote().use_localhost(false);
        sbn::kernel* k = nullptr;
        switch (command) {
            case Command::Submit: k = new Submit(argc, argv); break;
            case Command::Status: k = new Status; break;
            default: break;
        }
        sbnc::factory.local().send(k);
    } catch (const std::exception& err) {
        message("failed to connect to daemon process: _", err.what());
        sbn::graceful_shutdown(1);
    }
    auto ret = sbn::wait_and_return();
    sbnc::factory.stop();
    sbnc::factory.wait();
    return ret;
}
