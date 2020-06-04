#include <subordination/core/basic_factory.hh>

#include <unistdx/util/system>

sbn::Factory::Factory():
#if defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
_local(1) {
#else
_local(sys::thread_concurrency()) {
#endif
    this->_local.name("upstrm");
    this->_local.error_pipeline(&this->_remote);
    this->_remote.name("chld");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_remote);
    this->_remote.remote_pipeline(&this->_remote);
}

void sbn::Factory::start() {
    this->setstate(pipeline_state::starting);
    this->_local.start();
    this->_remote.start();
    this->setstate(pipeline_state::started);
}

void sbn::Factory::stop() {
    this->setstate(pipeline_state::stopping);
    this->_local.stop();
    this->_remote.stop();
    this->setstate(pipeline_state::stopped);
}

void sbn::Factory::wait() {
    this->_local.wait();
    this->_remote.wait();
}

void sbn::Factory::clear() {
    this->_local.clear();
    this->_remote.clear();
}

sbn::Factory sbn::factory;
