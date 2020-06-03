#include <subordination/ppl/application.hh>
#include <subordination/ppl/child_process_pipeline.hh>

void sbn::child_process_pipeline::send(kernel* k) {
    #ifndef NDEBUG
    this->log("send _", *k);
    #endif
    lock_type lock(this->_mutex);
    if (!this->_parent) {
        lock.unlock();
        this->_protocol.native_pipeline()->send(k);
    } else {
        traits_type::push(this->_kernels, k);
        this->poller().notify_one();
    }
}

sbn::child_process_pipeline::child_process_pipeline() {
    using namespace std::chrono;
    this->set_start_timeout(seconds(7));
    this->set_name("chld");
    using f = kernel_proto_flag;
    this->_protocol.setf(f::prepend_source_and_destination | f::save_upstream_kernels);
    sys::fd_type in = this_application::get_input_fd();
    sys::fd_type out = this_application::get_output_fd();
    if (in != -1 && out != -1) {
        this->_parent =
            std::make_shared<event_handler_type>(sys::pipe(in, out));
        this->_parent->protocol(&this->_protocol);
        this->_parent->setstate(pipeline_state::starting);
        this->_parent->set_name(this->name());
        this->emplace_handler(
            sys::epoll_event(in, sys::event::in),
            this->_parent
        );
        this->emplace_handler(
            sys::epoll_event(out, sys::event::out),
            this->_parent
        );
    }
}

void sbn::child_process_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto* k = this->_kernels.front();
        this->_kernels.pop();
        if (this->_parent && this->_parent->is_running()) {
            this->_parent->send(k);
        } else {
            this->_protocol.native_pipeline()->send(k);
        }
    }
}
