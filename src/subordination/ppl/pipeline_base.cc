#include <subordination/ppl/pipeline_base.hh>

void sbn::pipeline::forward(foreign_kernel* k) {
    this->log("forward is not implemented, deleting _", k);
    delete k;
}
