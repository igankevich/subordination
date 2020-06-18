#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistdx/fs/canonical_path>

#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/job_status_kernel.hh>
#include <subordination/daemon/pipeline_status_kernel.hh>
#include <subordination/daemon/small_factory.hh>
#include <subordination/daemon/status_kernel.hh>

enum class Command { Submit, Status, Delete };

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("sbnc", args ...);
}

template <class T> void
rec(std::ostream& out, const char* key, const T& value) {
    out << key << ": " << value << '\n';
}

template <class T>
class Rec {
private:
    const T& _object;
public:
    inline Rec(T object): _object(object) {}
    inline const T& object() const noexcept { return this->_object; }
};

template <class T> inline Rec<T> make_rec(const T& rhs) { return Rec<T>(rhs); }

std::ostream& operator<<(std::ostream& out, const Rec<sbnd::Status_kernel::hierarchy_type>& rhs) {
    const auto& h = rhs.object();
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
    return out << '\n';
}

std::ostream& operator<<(std::ostream& out, const Rec<sbn::application>& rhs) {
    const auto& a = rhs.object();
    rec(out, "id", a.id());
    rec(out, "user", a.user());
    rec(out, "group", a.group());
    out << "arguments: ";
    const auto& args = a.arguments();
    if (!args.empty()) {
        auto first = args.begin(), last = args.end();
        out << *first;
        while (first != last) {
            out << ' ' << *first;
            ++first;
        }
    }
    return out << '\n';
}

std::ostream& operator<<(std::ostream& out, const Rec<sbnd::Pipeline_status_kernel::Pipeline>& rhs) {
    const auto& ppl = rhs.object();
    for (const auto& conn : ppl.connections) {
        for (const auto& k : conn.kernels) {
            rec(out, "pipeline-name", ppl.name);
            rec(out, "connection-socket-address", conn.address);
            rec(out, "id", k.id);
            rec(out, "type-id", k.type_id);
            rec(out, "source-application-id", k.source_application_id);
            rec(out, "target-application-id", k.target_application_id);
            rec(out, "source-socket-address", k.source);
            rec(out, "destination-socket-address", k.destination);
        }
    }
    return out << '\n';
}

template <class T>
class Human {
private:
    const T& _object;
public:
    inline Human(T object): _object(object) {}
    inline const T& object() const noexcept { return this->_object; }
};

template <class T> inline Human<T> make_human(const T& rhs) { return Human<T>(rhs); }

template <class T> constexpr int column_width(const T&) {
    return std::numeric_limits<T>::digits10 + 1;
}

template <> constexpr int column_width(const sys::interface_address<sys::ipv4_address>&) {
    return 3*4 + 3 + 1 + 2 + 1;
}

template <> constexpr int column_width(const sys::interface_address<sys::ipv6_address>&) {
    return 4*8 + 7 + 1 + 2 + 1;
}

template <> constexpr int column_width(const sbnd::hierarchy_node&) {
    return 3*4 + 3 + 1 + 5 + 1 + std::numeric_limits<sbnd::hierarchy_node::weight_type>::digits10;
}

std::ostream& operator<<(std::ostream& out, const Human<sbnd::Status_kernel::hierarchy_type>& rhs) {
    const auto& h = rhs.object();
    out << std::left << std::setw(column_width(h.interface_address())) << h.interface_address();
    out << std::left << std::setw(column_width(h.principal())) << h.principal();
    if (!h.subordinates().empty()) {
        auto first = h.subordinates().begin();
        auto last = h.subordinates().end();
        out << *first;
        while (first != last) {
            out << ' ' << *first;
            ++first;
        }
    }
    return out << '\n';
}

std::ostream& operator<<(std::ostream& out, const Human<sbnd::Status_kernel::hierarchy_array>& rhs) {
    const auto& hierarchies = rhs.object();
    if (!hierarchies.empty()) {
        const auto& h = hierarchies.front();
        out << std::left << std::setw(std::max(column_width(h.interface_address()), 18)) << "Interface address";
        out << std::left << std::setw(std::max(column_width(h.principal()), 10)) << "Principal";
        out << "Subordinates";
        out << '\n';
    }
    for (const auto& h : hierarchies) { std::cout << make_human(h); }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Human<sbn::application>& rhs) {
    const auto& a = rhs.object();
    out << std::left << std::setw(column_width(a.id())) << a.id();
    out << std::left << std::setw(column_width(a.user())) << a.user();
    out << std::left << std::setw(column_width(a.group())) << a.group();
    const auto& args = a.arguments();
    if (!args.empty()) {
        auto first = args.begin(), last = args.end();
        out << *first;
        while (first != last) {
            out << ' ' << *first;
            ++first;
        }
    }
    return out << '\n';
}

std::ostream& operator<<(std::ostream& out, const Human<sbnd::Job_status_kernel::application_array>& rhs) {
    const auto& jobs = rhs.object();
    if (!jobs.empty()) {
        const auto& j = jobs.front();
        out << std::left << std::setw(column_width(j.id())) << "Id";
        out << std::left << std::setw(column_width(j.user())) << "User";
        out << std::left << std::setw(column_width(j.group())) << "Group";
        out << "Arguments";
        out << '\n';
    }
    for (const auto& j : jobs) { out << make_human(j); }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Human<sbnd::Pipeline_status_kernel::Pipeline>& rhs) {
    return out << make_rec(rhs.object());
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
    Submit(int argc, char** argv): _argc(argc), _argv(argv) {}

    void act() override {
        auto* k = new_application_kernel(this->_argc, this->_argv);
        k->parent(this);
        sbnc::factory.remote().send(k);
    }

    void react(kernel* child) override {
        auto* k = dynamic_cast<Main_kernel*>(child);
        if (k->return_code() != sbn::exit_code::success) {
            message("failed to submit _", k->source_application_id());
        } else {
            message("submitted _", k->source_application_id());
        }
        return_to_parent(k->return_code());
        sbnc::factory.local().send(this);
    }

};

enum class Entity_type { Node, Job, Kernel };

const char* to_string(Entity_type rhs) {
    switch (rhs) {
        case Entity_type::Node: return "node";
        case Entity_type::Job: return "job";
        case Entity_type::Kernel: return "kernel";
        default: return nullptr;
    }
}

std::ostream& operator<<(std::ostream& out, Entity_type rhs) {
    if (const auto* s = to_string(rhs)) { out << s; }
    else { out << static_cast<int>(rhs); }
    return out;
}

Entity_type string_to_entity_type(const std::string& s) {
    Entity_type rhs{};
    if (s == "node") { rhs = Entity_type::Node; }
    else if (s == "job") { rhs = Entity_type::Job; }
    else if (s == "kernel") { rhs = Entity_type::Kernel; }
    else throw std::invalid_argument("bad entity type");
    return rhs;
}

std::istream& operator>>(std::istream& in, Entity_type& rhs) {
    std::string s;
    in >> s;
    rhs = string_to_entity_type(s);
    return in;
}

enum class Format { Human, Rec };

const char* to_string(Format rhs) {
    switch (rhs) {
        case Format::Human: return "human";
        case Format::Rec: return "rec";
        default: return nullptr;
    }
}

Format string_to_format(const std::string& s) {
    Format rhs{};
    if (s == "human") { rhs = Format::Human; }
    else if (s == "rec") { rhs = Format::Rec; }
    else throw std::invalid_argument("bad format");
    return rhs;
}

std::ostream& operator<<(std::ostream& out, Format rhs) {
    if (const auto* s = to_string(rhs)) { out << s; }
    else { out << static_cast<int>(rhs); }
    return out;
}

std::istream& operator>>(std::istream& in, Format& rhs) {
    std::string s;
    in >> s;
    rhs = string_to_format(s);
    return in;
}

class Status: public sbn::kernel {

private:
    Entity_type _entity_type = Entity_type::Node;
    Format _output_format = Format::Human;
    sbnd::Job_status_kernel::application_id_array _job_ids;

public:
    Status(int argc, char** argv) {
        bool inside_d = false;
        for (int i=1; i<argc; ++i) {
            std::string argument(argv[i]);
            if (inside_d) {
                if (!argument.empty() && argument.front() == '-') {
                    inside_d = false;
                } else {
                    this->_job_ids.emplace_back(std::stoul(argv[i]));
                }
            }
            if (!inside_d) {
                if (argument == "-t") {
                    if (i+1 == argc) { throw std::invalid_argument("bad argument \"-t\""); }
                    this->_entity_type = string_to_entity_type(argv[i+1]);
                } else if (argument == "-o") {
                    if (i+1 == argc) { throw std::invalid_argument("bad argument \"-o\""); }
                    this->_output_format = string_to_format(argv[i+1]);
                } else if (argument == "-d") {
                    if (i+1 == argc) { throw std::invalid_argument("bad argument \"-d\""); }
                    this->_job_ids.emplace_back(std::stoul(argv[i+1]));
                    inside_d = true;
                } else {
                    std::stringstream tmp;
                    tmp << "unknown argument \"" << argument << "\"";
                    throw std::invalid_argument(tmp.str());
                }
            }
        }
    }

    void act() override {
        sbn::kernel* k = nullptr;
        switch (this->_entity_type) {
            case Entity_type::Node: k = new sbnd::Status_kernel; break;
            case Entity_type::Job: k = new sbnd::Job_status_kernel(this->_job_ids); break;
            case Entity_type::Kernel: k = new sbnd::Pipeline_status_kernel; break;
        }
        k->destination(sys::socket_address(SBND_SOCKET));
        k->principal_id(1); // TODO
        k->parent(this);
        sbnc::factory.remote().send(k);
    }

    void react(sbn::kernel* child) override {
        if (child->return_code() != sbn::exit_code::success) {
            message("error: _", child->return_code());
            return;
        }
        if (typeid(*child) == typeid(sbnd::Status_kernel)) {
            auto* k = dynamic_cast<sbnd::Status_kernel*>(child);
            switch (this->_output_format) {
                case Format::Human:
                    std::cout << k->hierarchies();
                    break;
                case Format::Rec:
                    for (const auto& h : k->hierarchies()) { std::cout << make_rec(h); }
                    break;
            }
        } else if (typeid(*child) == typeid(sbnd::Job_status_kernel)) {
            auto* k = dynamic_cast<sbnd::Job_status_kernel*>(child);
            switch (this->_output_format) {
                case Format::Human:
                    std::cout << k->jobs();
                    break;
                case Format::Rec:
                    for (const auto& j : k->jobs()) { std::cout << make_rec(j); }
                    break;
            }
        } else if (typeid(*child) == typeid(sbnd::Pipeline_status_kernel)) {
            auto* k = dynamic_cast<sbnd::Pipeline_status_kernel*>(child);
            switch (this->_output_format) {
                case Format::Human:
                    for (const auto& j : k->pipelines()) { std::cout << make_human(j); }
                    break;
                case Format::Rec:
                    for (const auto& j : k->pipelines()) { std::cout << make_rec(j); }
                    break;
            }
        }
        return_to_parent(child->return_code());
        sbnc::factory.local().send(this);
    }

};

void usage(char* argv[0]) {
    std::cerr << "usage: " << argv[0] <<
        " [-h] [-t entity-type] [-o output-format] [-d ids...] [command] [arguments...]\n"
        "-t type            entity type (node, job, kernel)\n"
        "-o format          output format (human, rec)\n"
        "-d ids...          delete entity (job)\n"
        "-h                 usage\n"
        "command arguments  a command with arguments to run\n";
}

int main(int argc, char* argv[]) {
    auto command = Command::Submit;
    if (argc <= 1) { usage(argv); return 1; }
    if (argc == 2 && std::string(argv[1]) ==  "-h") { usage(argv); return 0; }
    if (argc >= 2 && argv[1][0] == '-') { command = Command::Status; }
    if (sbn::this_application::id() == 0) {
        sbn::this_application::id(1);
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
            case Command::Status: k = new Status(argc, argv); break;
            default: break;
        }
        sbnc::factory.local().send(k);
    } catch (const std::exception& err) {
        message("failed to connect to daemon process: _", err.what());
        sbn::exit(1);
    }
    auto ret = sbn::wait_and_return();
    sbnc::factory.stop();
    sbnc::factory.wait();
    return ret;
}
