#include <sstream>

#include <unistdx/base/log_message>
#include <unistdx/fs/canonical_path>
#include <unistdx/fs/mkdirs>

#include <subordination/daemon/factory.hh>

namespace  {

    template <class T> inline bool is_set(T value) {
        return value != std::numeric_limits<T>::max();
    }

}

void sbnd::Properties::property(const std::string& key, const std::string& value) {
    if (key == "local.num-upstream-threads") {
        auto n = std::stoi(value);
        if (n < 1) { throw std::out_of_range("out of range"); }
        local.num_upstream_threads = n;
    } else if (key == "local.num-downstream-threads") {
        auto n = std::stoi(value);
        if (n < 0) { throw std::out_of_range("out of range"); }
        local.num_downstream_threads = n;
    } else if (key == "remote.min-input-buffer-size") {
        remote.min_input_buffer_size = std::stoul(value);
    } else if (key == "remote.min-output-buffer-size") {
        remote.min_output_buffer_size = std::stoul(value);
    } else if (key == "remote.max-connection-attempts") {
        auto v = std::stoul(value);
        if (v > std::numeric_limits<sys::u32>::max()) {
            throw std::out_of_range("out of range");
        }
        remote.max_connection_attempts = static_cast<sys::u32>(v);
    } else if (key == "remote.connection-timeout") {
        remote.connection_timeout = sbn::string_to_duration(value);
    } else if (key == "process.min-input-buffer-size") {
        process.min_input_buffer_size = std::stoul(value);
    } else if (key == "process.min-output-buffer-size") {
        process.min_output_buffer_size = std::stoul(value);
    } else if (key == "process.allow-root") {
        process.allow_root = sbn::string_to_bool(value);
    } else if (key == "unix.min-input-buffer-size") {
        unix.min_input_buffer_size = std::stoul(value);
    } else if (key == "unix.min-output-buffer-size") {
        unix.min_output_buffer_size = std::stoul(value);
    } else if (key == "factory.flags") {
        factory.flags = string_to_factory_flags(value);
    } else if (key == "transactions.directory") {
        transactions.directory = value;
    } else if (key == "transactions.recover-after") {
        transactions.recover_after = sbn::string_to_duration(value);
    } else if (key == "config") {
        open(value.data());
    } else if (key == "network.allowed-interface-addresses") {
        network.allowed_interface_addresses = string_to_interface_address_list(value);
    } else if (key == "network.interface-update-interval") {
        network.interface_update_interval = sbn::string_to_duration(value);
    } else if (key == "discoverer.scan-interval") {
        discoverer.scan_interval = sbn::string_to_duration(value);
    } else if (key == "discoverer.fanout") {
        auto v = std::stoul(value);
        if (v > std::numeric_limits<sys::ipv4_address::rep_type>::max() ||
            v == 0) {
            throw std::out_of_range("out of range");
        }
        discoverer.fanout = static_cast<sys::ipv4_address::rep_type>(v);
    } else if (key == "discoverer.max-attempts") {
        auto v = std::stoul(value);
        if (v > std::numeric_limits<int>::max() || v == 0) {
            throw std::out_of_range("out of range");
        }
        discoverer.max_attempts = static_cast<int>(v);
    } else if (key == "discoverer.cache-directory") {
        discoverer.cache_directory = value;
    } else if (key == "discoverer.profile") {
        discoverer.profile = sbn::string_to_bool(value);
    #if defined(SBND_WITH_GLUSTERFS)
    } else if (key == "glusterfs.working-directory") {
        glusterfs.working_directory = value;
    #endif
    } else {
        throw std::invalid_argument("unknown property");
    }
}

sbnd::Properties::Properties() {
    local.num_upstream_threads = sys::thread_concurrency();
    auto page_size = sys::page_size();
    remote.min_input_buffer_size = page_size*52;
    remote.min_output_buffer_size = page_size*52;
    process.min_input_buffer_size = page_size*16;
    process.min_output_buffer_size = page_size*16;
    unix.min_input_buffer_size = page_size*16;
    unix.min_output_buffer_size = page_size*16;
}

sbnd::Factory::Factory(): Factory(sys::thread_concurrency()) {}

sbnd::Factory::Factory(unsigned concurrency): _local(concurrency) {
    this->_local.name("sbnd local");
    this->_remote.name("sbnd remote");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_process);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.types(&this->_types);
    this->_remote.instances(&this->_instances);
    this->_process.name("sbnd proc");
    this->_process.native_pipeline(&this->_local);
    this->_process.foreign_pipeline(&this->_remote);
    this->_process.remote_pipeline(&this->_remote);
    this->_process.types(&this->_types);
    this->_process.instances(&this->_instances);
    this->_process.unix(&this->_unix);
    this->_unix.name("sbnd unix");
    this->_unix.native_pipeline(&this->_local);
    this->_unix.foreign_pipeline(&this->_process);
    this->_unix.remote_pipeline(&this->_remote);
    this->_unix.types(&this->_types);
    this->_unix.instances(&this->_instances);
}

void sbnd::Factory::transactions(const char* filename) {
    if (!isset(factory_flags::transactions)) {
        throw std::runtime_error("transactions are disabled");
    }
    this->_transactions.pipelines({&this->_remote,&this->_process,&this->_unix});
    this->_transactions.timer_pipeline(&this->_local);
    this->_transactions.types(&this->_types);
    this->_transactions.open(filename);
    this->_remote.transactions(&this->_transactions);
    this->_process.transactions(&this->_transactions);
    this->_unix.transactions(&this->_transactions);
}

void sbnd::Factory::start() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local.start(); }
    if (isset(f::remote)) { this->_remote.start(); }
    if (isset(f::process)) { this->_process.start(); }
    if (isset(f::unix)) { this->_unix.start(); }
}

void sbnd::Factory::stop() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local.stop(); }
    if (isset(f::remote)) { this->_remote.stop(); }
    if (isset(f::process)) { this->_process.stop(); }
    if (isset(f::unix)) { this->_unix.stop(); }
}

void sbnd::Factory::wait() {
    using f = factory_flags;
    if (isset(f::local)) { this->_local.wait(); }
    if (isset(f::remote)) { this->_remote.wait(); }
    if (isset(f::process)) { this->_process.wait(); }
    if (isset(f::unix)) { this->_unix.wait(); }
}

void sbnd::Factory::clear() {
    using f = factory_flags;
    sbn::kernel_sack sack;
    if (isset(f::local)) { this->_local.clear(sack); }
    if (isset(f::remote)) { this->_remote.clear(sack); }
    if (isset(f::process)) { this->_process.clear(sack); }
    if (isset(f::unix)) { this->_unix.clear(sack); }
    this->_instances.clear(sack);
}

void sbnd::Factory::configure(const Properties& props) {
    using f = factory_flags;
    this->_flags = props.factory.flags;
    if (isset(f::local)) {
        this->_local.num_upstream_threads(props.local.num_upstream_threads);
        this->_local.num_downstream_threads(props.local.num_downstream_threads);
    }
    if (isset(f::remote)) {
        this->_remote.min_output_buffer_size(props.remote.min_output_buffer_size);
        this->_remote.min_input_buffer_size(props.remote.min_input_buffer_size);
        this->_remote.max_connection_attempts(props.remote.max_connection_attempts);
        this->_remote.connection_timeout(props.remote.connection_timeout);
    }
    if (isset(f::process)) {
        this->_process.min_output_buffer_size(props.process.min_output_buffer_size);
        this->_process.min_input_buffer_size(props.process.min_input_buffer_size);
        this->_process.allow_root(props.process.allow_root);
    }
    if (isset(f::unix)) {
        this->_unix.min_output_buffer_size(props.unix.min_output_buffer_size);
        this->_unix.min_input_buffer_size(props.unix.min_input_buffer_size);
    }
    if (isset(f::transactions)) {
        this->_transactions.recover_after(props.transactions.recover_after);
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
    enum { initial, negative, positive } state = initial;
    char ch;
    std::string name;
    while (!in.get(ch).eof()) {
        switch (state) {
            case initial:
                if (ch == '-') { state = negative; }
                else if (ch == '+') { state = positive; }
                else if (ch == ' ') { /* skip white space */ }
                else throw std::invalid_argument("bad factory flags");
                break;
            case negative:
            case positive:
                if (ch == '+' || ch == '-' || ch == ' ') {
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
        }
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
