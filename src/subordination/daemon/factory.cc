#include <subordination/daemon/factory.hh>

sbnd::Factory::Factory(): Factory(sys::thread_concurrency()) {}

sbnd::Factory::Factory(unsigned concurrency): _local(concurrency) {
    this->_local.name("upstrm");
    this->_remote.name("nic");
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_process.name("proc");
    this->_external.name("unix");
    #endif
    this->_process.set_other_mutex(this->_remote.mutex());
    this->_process.native_pipeline(&this->_local);
    this->_process.foreign_pipeline(&this->_remote);
    this->_process.remote_pipeline(&this->_remote);
    this->_process.types(&this->_types);
    this->_process.instances(&this->_instances);
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_process);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.set_other_mutex(this->_process.mutex());
    this->_remote.types(&this->_types);
    this->_remote.instances(&this->_instances);
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_external.native_pipeline(&this->_local);
    this->_external.foreign_pipeline(&this->_process);
    this->_external.remote_pipeline(&this->_remote);
    this->_external.types(&this->_types);
    this->_external.instances(&this->_instances);
    #endif
}

sbnd::Factory sbnd::factory{};
