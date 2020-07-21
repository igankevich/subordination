#include <unistdx/base/log_message>

#include <subordination/daemon/factory.hh>

sbnd::Factory::Factory(): Factory(sys::thread_concurrency()) {}

sbnd::Factory::Factory(unsigned concurrency): _local(concurrency) {
    this->_local.name("upstrm");
    this->_remote.name("nic");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_process);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.types(&this->_types);
    this->_remote.instances(&this->_instances);
    this->_process.name("proc");
    this->_process.native_pipeline(&this->_local);
    this->_process.foreign_pipeline(&this->_remote);
    this->_process.remote_pipeline(&this->_remote);
    this->_process.types(&this->_types);
    this->_process.instances(&this->_instances);
    this->_process.unix(&this->_unix);
    this->_unix.name("unix");
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


sbnd::Factory sbnd::factory{};
