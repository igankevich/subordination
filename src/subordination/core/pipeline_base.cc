#include <subordination/core/pipeline_base.hh>

void sbn::pipeline::forward(foreign_kernel_ptr&& k) {
    this->log("forward is not implemented, deleting _", *k);
}

void sbn::pipeline::recover(kernel_ptr&& k) {
    this->log("recover is not implemented, deleting _", *k);
}
