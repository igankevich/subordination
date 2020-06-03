#include <subordination/ppl/basic_factory.hh>

#include <unistdx/util/system>

namespace {

    inline void
    start_all() {}

    template <class Pipeline, class ... Args>
    void
    start_all(Pipeline& ppl, Args& ... args) {
        ppl.start();
        start_all(args ...);
    }

    inline void
    stop_all() {}

    template <class Pipeline, class ... Args>
    void
    stop_all(Pipeline& ppl, Args& ... args) {
        ppl.stop();
        stop_all(args ...);
    }

    inline void
    wait_all() {}

    template <class Pipeline, class ... Args>
    void
    wait_all(Pipeline& ppl, Args& ... args) {
        ppl.wait();
        wait_all(args ...);
    }

}

sbn::Factory::Factory():
#if defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
_native(1),
_downstream(1) {
#else
_native(sys::thread_concurrency()),
_downstream(_native.concurrency()) {
#endif
    this->_native.set_name("upstrm");
    this->_downstream.set_name("dwnstrm");
    this->_scheduled.set_name("tmr");
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_io.set_name("io");
    #endif
    this->_parent.set_name("chld");
    this->_parent.native_pipeline(&this->_native);
    this->_parent.foreign_pipeline(&this->_parent);
    this->_parent.remote_pipeline(&this->_parent);
}

void sbn::Factory::start() {
    this->setstate(pipeline_state::starting);
    start_all(
        this->_native,
        this->_downstream
        ,
        this->_scheduled
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
    );
    this->setstate(pipeline_state::started);
}

void sbn::Factory::stop() {
    this->setstate(pipeline_state::stopping);
    stop_all(
        this->_native,
        this->_downstream
        ,
        this->_scheduled
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
    );
    this->setstate(pipeline_state::stopped);
}

void sbn::Factory::wait() {
    wait_all(
        this->_native,
        this->_downstream
        ,
        this->_scheduled
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
    );
}

sbn::Factory sbn::factory;
