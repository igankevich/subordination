#include <unistdx/base/log_message>

#include <subordination/daemon/config.hh>
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
    //this->_remote.transactions(&this->_transactions);
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_process.name("proc");
    this->_process.native_pipeline(&this->_local);
    this->_process.foreign_pipeline(&this->_remote);
    this->_process.remote_pipeline(&this->_remote);
    this->_process.types(&this->_types);
    this->_process.instances(&this->_instances);
    this->_process.unix(&this->_unix);
    //this->_process.transactions(&this->_transactions);
    this->_unix.name("unix");
    this->_unix.native_pipeline(&this->_local);
    this->_unix.foreign_pipeline(&this->_process);
    this->_unix.remote_pipeline(&this->_remote);
    this->_unix.types(&this->_types);
    this->_unix.instances(&this->_instances);
    //this->_unix.transactions(&this->_transactions);
    #endif
    //this->_transactions.pipelines({&this->_remote,&this->_process,&this->_unix});
    //this->_transactions.types(&this->_types);
    //this->_transactions.open(SUBORDINATION_SHARED_STATE_DIR "/transactions");
    //this->_transactions.open("transactions");
}

void sbnd::Factory::clear() {
    sbn::kernel_sack sack;
    this->_local.clear(sack);
    this->_remote.clear(sack);
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_process.clear(sack);
    this->_unix.clear(sack);
    #endif
    this->_instances.clear(sack);
}


sbnd::Factory sbnd::factory{};
