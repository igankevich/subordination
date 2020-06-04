#include <unistdx/io/pipe>

#include <subordination/core/application.hh>
#include <subordination/core/child_process_pipeline.hh>

void sbn::child_process_pipeline::send(kernel* k) {
    #if defined(SBN_DEBUG)
    this->log("send _", *k);
    #endif
    lock_type lock(this->_mutex);
    if (!this->_parent) {
        lock.unlock();
        native_pipeline()->send(k);
    } else {
        this->_kernels.emplace(k);
        this->poller().notify_one();
    }
}

sbn::child_process_pipeline::child_process_pipeline() {
    using namespace std::chrono;
    this->set_start_timeout(seconds(7));
    this->name("chld");
    using f = kernel_proto_flag;
    sys::fd_type in = this_application::get_input_fd();
    sys::fd_type out = this_application::get_output_fd();
    if (in != -1 && out != -1) {
        this->_parent =
            std::make_shared<event_handler_type>(sys::pipe(in, out));
        this->_parent->parent(this);
        this->_parent->setf(f::prepend_source_and_destination | f::save_upstream_kernels);
        this->_parent->setstate(pipeline_state::starting);
        this->_parent->name(this->name());
        this->emplace_handler(sys::epoll_event(in, sys::event::in), this->_parent);
        this->emplace_handler(sys::epoll_event(out, sys::event::out), this->_parent);
    }
}

void sbn::child_process_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto* k = this->_kernels.front();
        this->_kernels.pop();
        if (this->_parent && this->_parent->is_running()) {
            this->_parent->send(k);
        } else {
            native_pipeline()->send(k);
        }
    }
}
