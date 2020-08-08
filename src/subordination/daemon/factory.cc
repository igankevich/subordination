#include <unistdx/base/log_message>

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
    } else if (key == "process.min-input-buffer-size") {
        process.min_input_buffer_size = std::stoul(value);
    } else if (key == "process.min-output-buffer-size") {
        process.min_output_buffer_size = std::stoul(value);
    } else if (key == "unix.min-input-buffer-size") {
        unix.min_input_buffer_size = std::stoul(value);
    } else if (key == "unix.min-output-buffer-size") {
        unix.min_output_buffer_size = std::stoul(value);
    } else {
        throw std::runtime_error("unknown property");
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
    if (isset(f::local)) {
        std::clog << "props.local.num_upstream_threads=" << props.local.num_upstream_threads << std::endl;
        this->_local.num_upstream_threads(props.local.num_upstream_threads);
        this->_local.num_downstream_threads(props.local.num_downstream_threads);
    }
    if (isset(f::remote)) {
        this->_remote.min_output_buffer_size(props.remote.min_output_buffer_size);
        this->_remote.min_input_buffer_size(props.remote.min_input_buffer_size);
    }
    if (isset(f::process)) {
        this->_process.min_output_buffer_size(props.process.min_output_buffer_size);
        this->_process.min_input_buffer_size(props.process.min_input_buffer_size);
    }
    if (isset(f::unix)) {
        this->_unix.min_output_buffer_size(props.unix.min_output_buffer_size);
        this->_unix.min_input_buffer_size(props.unix.min_input_buffer_size);
    }
}

sbnd::Factory sbnd::factory{};
