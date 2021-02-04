#include <sstream>

#include <unistdx/base/log_message>
#include <unistdx/fs/canonical_path>
#include <unistdx/fs/mkdirs>

#include <subordination/daemon/factory.hh>

namespace {
    template <class Properties>
    inline bool set_if_prefix(const std::string& prefix,
                              Properties& properties,
                              const std::string& key,
                              const std::string& value) {
        if (key.compare(0, prefix.size(), prefix) == 0) {
            return properties.set(key.data() + prefix.size(), value);
        }
        return false;
    }
}

void sbnd::Properties::property(const std::string& key, const std::string& value) {
    std::string local_prefix{"local."};
    if (key == "config") {
        open(value.data());
    } else if (set_if_prefix("local.", local, key, value)) {
    } else if (set_if_prefix("remote.", remote, key, value)) {
    } else if (set_if_prefix("process.", process, key, value)) {
    } else if (set_if_prefix("unix.", unix, key, value)) {
    } else if (set_if_prefix("discoverer.", discover, key, value)) {
    } else if (set_if_prefix("transactions.", transactions, key, value)) {
    } else if (key == "factory.flags") {
        factory.flags = string_to_factory_flags(value);
    } else if (key == "network.allowed-interface-addresses") {
        network.allowed_interface_addresses = string_to_interface_address_list(value);
    } else if (key == "network.interface-update-interval") {
        network.interface_update_interval = sbn::string_to_duration(value);
    } else if (key.compare(0, 10, "resources.") == 0) {
        std::string name = key.substr(10);
        if (!sbn::resources::is_valid_name(name.data(), name.data()+name.size())) {
            throw std::invalid_argument("bad symbol name");
        }
        resources.expressions[name] = sbn::resources::read(value, 10);
    #if defined(SBND_WITH_GLUSTERFS)
    } else if (key == "glusterfs.working-directory") {
        glusterfs.working_directory = value;
    #endif
    } else {
        throw std::invalid_argument("unknown property");
    }
}

sbnd::Properties::Properties(const sys::cpu_set& cpus, size_t page_size):
local{cpus}, remote{cpus,page_size}, process{cpus,page_size}, unix{cpus,page_size} {
    transactions.directory = sys::path{SBND_SHARED_STATE_DIR};
    discover.cache_directory = sys::path{SBND_SHARED_STATE_DIR};
}

void sbnd::Factory::transactions(const char* filename) {
    if (!isset(factory_flags::transactions)) {
        throw std::runtime_error("transactions are disabled");
    }
    this->_transactions->pipelines({&this->_remote,&this->_process,&this->_unix});
    this->_transactions->timer_pipeline(&this->_local);
    this->_transactions->types(&this->_types);
    this->_transactions->open(filename);
    this->_remote->transactions(&this->_transactions);
    this->_process->transactions(&this->_transactions);
    this->_unix->transactions(&this->_transactions);
}

void sbnd::Factory::start() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local->start(); }
    if (isset(f::remote)) { this->_remote->start(); }
    if (isset(f::process)) { this->_process->start(); }
    if (isset(f::unix)) { this->_unix->start(); }
}

void sbnd::Factory::stop() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local->stop(); }
    if (isset(f::remote)) { this->_remote->stop(); }
    if (isset(f::process)) { this->_process->stop(); }
    if (isset(f::unix)) { this->_unix->stop(); }
}

void sbnd::Factory::wait() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local->wait(); }
    if (isset(f::remote)) { this->_remote->wait(); }
    if (isset(f::process)) { this->_process->wait(); }
    if (isset(f::unix)) { this->_unix->wait(); }
}

void sbnd::Factory::clear() {
    using f = factory_flags;
    sbn::kernel_sack sack;
    if (isset(f::local)) { this->_local->clear(sack); }
    if (isset(f::remote)) { this->_remote->clear(sack); }
    if (isset(f::process)) { this->_process->clear(sack); }
    if (isset(f::unix)) { this->_unix->clear(sack); }
    this->_instances.clear(sack);
}

void sbnd::Factory::configure(const Properties& props) {
    using f = factory_flags;
    this->_flags = props.factory.flags;
    if (isset(f::local)) {
        this->_local.make(props.local);
        this->_local->name("sbnd local");
    }
    if (isset(f::remote)) {
        this->_remote.make(props.remote);
        this->_remote->name("sbnd remote");
        this->_remote->native_pipeline(&this->_local);
        this->_remote->foreign_pipeline(&this->_process);
        this->_remote->remote_pipeline(&this->_remote);
        this->_remote->types(&this->_types);
        this->_remote->instances(&this->_instances);
    }
    if (isset(f::process)) {
        this->_process.make(props.process);
        this->_process->name("sbnd proc");
        this->_process->native_pipeline(&this->_local);
        this->_process->foreign_pipeline(&this->_remote);
        this->_process->remote_pipeline(&this->_remote);
        this->_process->types(&this->_types);
        this->_process->instances(&this->_instances);
        this->_process->unix(&this->_unix);
    }
    if (isset(f::unix)) {
        this->_unix.make(props.unix);
        this->_unix->name("sbnd unix");
        this->_unix->native_pipeline(&this->_local);
        this->_unix->foreign_pipeline(&this->_process);
        this->_unix->remote_pipeline(&this->_remote);
        this->_unix->types(&this->_types);
        this->_unix->instances(&this->_instances);
    }
    if (isset(f::transactions)) {
        this->_transactions.make();
        this->_transactions->recover_after(props.transactions.recover_after);
        sys::mkdirs(props.transactions.directory);
        transactions(sys::path(sys::canonical_path(props.transactions.directory), "transactions").data());
    }
}

sbnd::Factory sbnd::factory{};

auto sbnd::string_to_factory_flags(const std::string& s) -> factory_flags {
    factory_flags flags = factory_flags::all;
    std::stringstream tmp;
    tmp << s;
    tmp >> flags;
    return flags;
}

std::istream& sbnd::operator>>(std::istream& in, sbnd::factory_flags& rhs) {
    enum { initial, negative, positive, finished } state = initial;
    char ch;
    std::string name;
    while (state != finished) {
        ch = in.get();
        switch (state) {
            case initial:
                if (ch == '-') { state = negative; }
                else if (ch == '+') { state = positive; }
                else if (ch == ' ') { /* skip white space */ }
                else throw std::invalid_argument("bad factory flags");
                break;
            case negative:
            case positive:
                if (ch == '+' || ch == '-' || ch == ' ' || in.eof()) {
                    using f = sbnd::factory_flags;
                    f new_flag{};
                    if (name == "local") { new_flag = f::local; }
                    else if (name == "remote") { new_flag = f::remote; }
                    else if (name == "process") { new_flag = f::process; }
                    else if (name == "unix") { new_flag = f::unix; }
                    else if (name == "transactions") { new_flag = f::transactions; }
                    else if (name == "all") { new_flag = f::all; }
                    else throw std::invalid_argument("bad factory flags");
                    if (state == positive) { rhs |= new_flag; }
                    else { rhs &= ~new_flag; }
                    name.clear();
                    state = initial;
                } else {
                    name += ch;
                }
                break;
            default:
                break;
        }
        if (in.eof()) { state = finished; }
    }
    return in;
}

auto sbnd::string_to_interface_address_list(const std::string& s) -> interface_address_list {
    interface_address_list lst;
    std::stringstream tmp;
    std::string::size_type n = s.size(), i = 0, i0 = 0;
    for (; i<n; ++i) {
        if (s[i] == ',' || i == n-1) {
            tmp.clear();
            tmp.str(s.substr(i0,i-i0+(i==n-1?1:0)));
            lst.emplace_back();
            tmp >> lst.back();
        }
    }
    return lst;
}
