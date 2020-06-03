#include <subordination/ppl/basic_socket_pipeline.hh>

void sbn::basic_socket_pipeline::collect_kernels(kernel_sack& sack) {
    while (!this->_kernels.empty()) {
        this->_kernels.front()->mark_as_deleted(sack);
        this->_kernels.pop();
    }
}
