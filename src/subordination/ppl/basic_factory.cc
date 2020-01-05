#include "basic_factory.hh"

#include <unistdx/util/system>

#include <subordination/config.hh>

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

template <class T>
sbn::Factory<T>
::Factory():
#if defined(SUBORDINATION_SUBMIT) || defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
_upstream(1),
_downstream(1) {
#else
_upstream(sys::thread_concurrency()),
_downstream(_upstream.concurrency()) {
#endif
    this->_upstream.set_name("upstrm");
    this->_downstream.set_name("dwnstrm");
    this->_timer.set_name("tmr");
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_io.set_name("io");
    #endif
    #if defined(SUBORDINATION_DAEMON)
    this->_parent.set_name("nic");
    #elif defined(SUBORDINATION_SUBMIT)
    this->_parent.set_name("chld");
    #endif
    #if defined(SUBORDINATION_DAEMON) && \
    !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_child.set_name("proc");
    this->_external.set_name("unix");
    #endif
    #if defined(SUBORDINATION_DAEMON)
    this->_child.set_other_mutex(this->_parent.mutex());
    this->_parent.set_other_mutex(this->_child.mutex());
    #endif
}

template <class T>
void
sbn::Factory<T>
::start() {
    this->setstate(pipeline_state::starting);
    start_all(
        this->_upstream,
        this->_downstream
        ,
        this->_timer
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
        #if defined(SUBORDINATION_DAEMON) && \
        !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_child
        ,
        this->_external
        #endif
    );
    this->setstate(pipeline_state::started);
}

template <class T>
void
sbn::Factory<T>
::stop() {
    this->setstate(pipeline_state::stopping);
    stop_all(
        this->_upstream,
        this->_downstream
        ,
        this->_timer
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
        #if defined(SUBORDINATION_DAEMON) && \
        !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_child
        ,
        this->_external
        #endif
    );
    this->setstate(pipeline_state::stopped);
}

template <class T>
void
sbn::Factory<T>
::wait() {
    wait_all(
        this->_upstream,
        this->_downstream
        ,
        this->_timer
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_io
        #endif
        ,
        this->_parent
        #if defined(SUBORDINATION_DAEMON) && \
        !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        ,
        this->_child
        ,
        this->_external
        #endif
    );
}

template <class T>
void
sbn::Factory<T>
::print_state(std::ostream& out) {
    this->_parent.print_state(out);
    #if defined(SUBORDINATION_DAEMON) && \
    !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_child.print_state(out);
    #endif
}

template class sbn::Factory<SUBORDINATION_KERNEL_TYPE>;
template class sbn::basic_router<SUBORDINATION_KERNEL_TYPE>;

sbn::Factory<SUBORDINATION_KERNEL_TYPE> sbn::factory;
