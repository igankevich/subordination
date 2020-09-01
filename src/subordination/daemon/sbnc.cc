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
#include <subordination/daemon/main_kernel.hh>
#include <subordination/daemon/pipeline_status_kernel.hh>
#include <subordination/daemon/small_factory.hh>
#include <subordination/daemon/status_kernel.hh>
#include <subordination/daemon/transaction_test_kernel.hh>

enum class Command { Submit, Status, Delete };

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("sbnc", args ...);
}

inline void
tell(std::ostream& out, const char* text) {
    out << text;
}

template <class Head, class ... Tail> inline void
tell(std::ostream& out, const char* text, const Head& head, const Tail& ... tail) {
    while (*text) {
        if (*text == '%') {
            out << head;
            tell(out, ++text, tail...);
            break;
        } else {
            out.put(*text);
        }
        ++text;
    }
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
    inline Rec(const T& object): _object(object) {}
    inline const T& object() const noexcept { return this->_object; }
};

template <class T> inline Rec<T> make_rec(const T& rhs) { return Rec<T>(rhs); }

std::ostream& operator<<(std::ostream& out, const Rec<sbnd::Status_kernel::hierarchy_type>& rhs) {
    const auto& h = rhs.object();
    rec(out, "interface-address", h.interface_address());
    rec(out, "superior", h.superior());
    out << "subordinates: ";
    if (!h.subordinates().empty()) {
        auto first = h.subordinates().begin();
        auto last = h.subordinates().end();
        out << *first;
        ++first;
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
    bool empty = true;
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
            empty = false;
        }
    }
    if (!empty) { out << '\n'; }
    return out;
}

template <class T>
class Human {
private:
    const T& _object;
public:
    inline Human(const T& object): _object(object) {}
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
    std::stringstream tmp;
    tmp << h.interface_address();
    out << std::left << std::setw(std::max(column_width(h.interface_address()), 18))
        << tmp.str();
    tmp.str("");
    tmp << h.superior();
    out << std::left << std::setw(std::max(column_width(h.superior()), 10)) << tmp.str();
    if (!h.subordinates().empty()) {
        auto first = h.subordinates().begin();
        auto last = h.subordinates().end();
        out << *first;
        ++first;
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
        out << std::left << std::setw(std::max(column_width(h.interface_address()), 18))
            << "Interface address";
        out << std::left << std::setw(std::max(column_width(h.superior()), 10))
            << "Superior";
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

class Submit: public sbn::kernel {

public:
    using string_array = sbn::application::string_array;

private:
    string_array _arguments;
    bool _test_recovery = false;

public:
    Submit(int argc, char** argv) {
        int i0 = 1;
        if (argc >= 2 && std::string(argv[1]) == "-T") {
            this->_test_recovery = true;
            i0 = 2;
        }
        for (int i=i0; i<argc; ++i) {
            this->_arguments.emplace_back(argv[i]);
        }
    }

    void act() override {
        sbn::kernel_ptr k;
        if (this->_test_recovery) {
            k = make_transaction_test_kernel();
        } else {
            k = make_application_kernel();
        }
        k->parent(this);
        sbnc::factory.remote().send(std::move(k));
    }

    void react(sbn::kernel_ptr&& child) override {
        const auto& t = typeid(*child);
        return_to_parent(child->return_code());
        if (t == typeid(sbnd::Main_kernel)) {
            auto k = sbn::pointer_dynamic_cast<sbnd::Main_kernel>(std::move(child));
            message("main kernel returned from _ with exit code _",
                    k->source_application_id(), k->return_code());
        } else {
            auto k =
                sbn::pointer_dynamic_cast<sbnd::Transaction_test_kernel>(std::move(child));
            int i = 0;
            for (auto ret : k->exit_codes()) {
                message("exit code #_: _", ++i, ret);
            }
        }
        sbnc::factory.local().send(std::move(this_ptr()));
    }

private:

    static string_array make_environment() {
        string_array env;
        for (char** first=environ; *first; ++first) { env.emplace_back(*first); }
        return env;
    }

    sbn::kernel_ptr make_application_kernel() {
        auto* app = new sbn::application(std::move(this->_arguments), make_environment());
        app->working_directory(sys::canonical_path("."));
        auto k = sbn::make_pointer<sbnd::Main_kernel>();
        k->target_application(app);
        //k->source_application_id(app->id());
        k->destination(sys::socket_address(SBND_SOCKET));
        return k;
    }

    sbn::kernel_ptr make_transaction_test_kernel() {
        sbn::application app(std::move(this->_arguments), make_environment());
        app.working_directory(sys::canonical_path("."));
        auto k = sbn::make_pointer<sbnd::Transaction_test_kernel>(std::move(app));
        k->target_application(0);
        k->destination(sys::socket_address(SBND_SOCKET));
        return k;
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
                    ++i;
                } else if (argument == "-o") {
                    if (i+1 == argc) { throw std::invalid_argument("bad argument \"-o\""); }
                    this->_output_format = string_to_format(argv[i+1]);
                    ++i;
                } else if (argument == "-d") {
                    if (i+1 == argc) { throw std::invalid_argument("bad argument \"-d\""); }
                    this->_job_ids.emplace_back(std::stoul(argv[i+1]));
                    ++i;
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
        sbn::kernel_ptr k;
        switch (this->_entity_type) {
            case Entity_type::Node: k = sbn::make_pointer<sbnd::Status_kernel>(); break;
            case Entity_type::Job: k = sbn::make_pointer<sbnd::Job_status_kernel>(this->_job_ids); break;
            case Entity_type::Kernel: k = sbn::make_pointer<sbnd::Pipeline_status_kernel>(); break;
        }
        k->phase(sbn::kernel::phases::point_to_point);
        k->destination(sys::socket_address(SBND_SOCKET));
        k->principal_id(1); // TODO
        k->parent(this);
        k->target_application_id(0);
        sbnc::factory.remote().send(std::move(k));
    }

    void react(sbn::kernel_ptr&& child) override {
        if (child->return_code() != sbn::exit_code::success) {
            return_code(child->return_code());
            if (child->return_code() == sbn::exit_code::endpoint_not_connected) {
                tell(std::cerr, "Unable to contact the daemon at %.\n",
                               sys::socket_address(SBND_SOCKET));
            } else {
                tell(std::cerr, "Error: %.\n", child->return_code());
            }
        } else {
            if (typeid(*child) == typeid(sbnd::Status_kernel)) {
                auto k = sbn::pointer_dynamic_cast<sbnd::Status_kernel>(std::move(child));
                switch (this->_output_format) {
                    case Format::Human:
                        std::cout << k->hierarchies();
                        break;
                    case Format::Rec:
                        for (const auto& h : k->hierarchies()) { std::cout << make_rec(h); }
                        break;
                }
            } else if (typeid(*child) == typeid(sbnd::Job_status_kernel)) {
                auto k = sbn::pointer_dynamic_cast<sbnd::Job_status_kernel>(std::move(child));
                switch (this->_output_format) {
                    case Format::Human:
                        std::cout << k->jobs();
                        break;
                    case Format::Rec:
                        for (const auto& j : k->jobs()) { std::cout << make_rec(j); }
                        break;
                }
            } else if (typeid(*child) == typeid(sbnd::Pipeline_status_kernel)) {
                auto k = sbn::pointer_dynamic_cast<sbnd::Pipeline_status_kernel>(std::move(child));
                switch (this->_output_format) {
                    case Format::Human:
                        for (const auto& j : k->pipelines()) { std::cout << make_human(j); }
                        break;
                    case Format::Rec:
                        for (const auto& j : k->pipelines()) { std::cout << make_rec(j); }
                        break;
                }
            }
        }
        return_to_parent();
        sbnc::factory.local().send(std::move(this_ptr()));
    }

};

void usage(char* argv[0]) {
    std::cerr << "usage: "
        "sbnc [-h] [-t entity-type] [-o output-format] [-d ids...]\n"
        "sbnc [-T] [command] [arguments...]\n"
        "-t type            entity type (node, job, kernel)\n"
        "-o format          output format (human, rec)\n"
        "-d ids...          delete entity (job)\n"
        "-h                 usage\n"
        "-T                 test an ability to recover from power failure\n"
        "command arguments  a command with arguments to run\n";
}

int main(int argc, char* argv[]) {
    auto command = Command::Submit;
    if (argc <= 1) { usage(argv); return 1; }
    if (argc == 2 && std::string(argv[1]) ==  "-h") { usage(argv); return 0; }
    if (argc >= 2 && argv[1][0] == '-' && std::string(argv[1]) != "-T") {
        command = Command::Status;
    }
    if (command == Command::Submit) {
        if (sbn::this_application::id() == 0) { sbn::this_application::id(1); }
    }
    sbn::install_error_handler();
    sbnc::factory.types().add<sbnd::Main_kernel>(1);
    sbnc::factory.types().add<sbnd::Status_kernel>(4);
    sbnc::factory.types().add<sbnd::Transaction_test_kernel>(6);
    sbnc::factory.types().add<sbnd::Job_status_kernel>(8);
    sbnc::factory.types().add<sbnd::Pipeline_status_kernel>(9);
    try {
        sbnc::factory.start();
        sbnc::factory.remote().scheduler().local(false);
        sbn::kernel_ptr k;
        switch (command) {
            case Command::Submit: k = sbn::make_pointer<Submit>(argc, argv); break;
            case Command::Status: k = sbn::make_pointer<Status>(argc, argv); break;
            default: break;
        }
        sbnc::factory.local().send(std::move(k));
    } catch (const std::exception& err) {
        tell(std::cerr, err.what());
        sbn::exit(1);
    }
    auto ret = sbn::wait_and_return();
    sbnc::factory.stop();
    sbnc::factory.wait();
    sbnc::factory.clear();
    return ret;
}
