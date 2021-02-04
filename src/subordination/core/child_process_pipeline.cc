#include <unistdx/io/pipe>

#include <subordination/bits/contracts.hh>
#include <subordination/core/application.hh>
#include <subordination/core/child_process_pipeline.hh>

void sbn::child_process_pipeline::send(kernel_ptr&& k) {
    Expects(k.get());
    #if defined(SBN_DEBUG)
    log("send _", *k);
    #endif
    lock_type lock(this->_mutex);
    if (!this->_parent) {
        send_native(std::move(k));
    } else {
        this->_kernels.emplace_back(std::move(k));
        this->poller().notify_one();
    }
}

void sbn::child_process_pipeline::add_connection() {
    using f = connection_flags;
    sys::fd_type in = this_application::get_input_fd();
    sys::fd_type out = this_application::get_output_fd();
    if (in != -1 && out != -1) {
        sys::pipe pipe(in, out);
        pipe.in().pipe_buffer_size(this->_pipe_buffer_size);
        pipe.out().pipe_buffer_size(this->_pipe_buffer_size);
        this->_parent = std::make_shared<process_handler>(std::move(pipe));
        this->_parent->parent(this);
        this->_parent->types(types());
        this->_parent->setf(f::save_upstream_kernels);
        this->_parent->state(connection::states::starting);
        this->_parent->name(name());
        this->_parent->add(this->_parent);
    }
}

void sbn::child_process_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto k = std::move(this->_kernels.front());
        this->_kernels.pop_front();
        if (this->_parent && (this->_parent->state() == connection::states::started ||
                              this->_parent->state() == connection::states::starting)) {
            this->_parent->send(k);
        } else {
            send_native(std::move(k));
        }
    }
}

sbn::child_process_pipeline::child_process_pipeline(const properties& p):
sbn::basic_socket_pipeline{p}, _pipe_buffer_size{p.pipe_buffer_size} {}

bool sbn::child_process_pipeline::properties::set(const char* key, const std::string& value) {
    bool found = true;
    if (basic_socket_pipeline::properties::set(key, value)) {
    } else if (std::strcmp(key, "pipe-buffer-size") == 0) {
        pipe_buffer_size = std::stoul(value);
    } else {
        found = false;
    }
    return found;
}
