#include <subordination/ppl/multi_pipeline.hh>

sbn::multi_pipeline::multi_pipeline(unsigned npipelines):
_pipelines(npipelines) {
    unsigned num = 0;
    for (auto& ppl : this->_pipelines) {
        ppl.set_number(num);
        ++num;
    }
}

void
sbn::multi_pipeline::set_name(const char* rhs) {
    for (auto& ppl : this->_pipelines) {
        ppl.set_name(rhs);
    }
}


void
sbn::multi_pipeline::start() {
    this->setstate(pipeline_state::starting);
    for (auto& ppl : this->_pipelines) {
        ppl.start();
    }
    this->setstate(pipeline_state::started);
}

void
sbn::multi_pipeline::stop() {
    this->setstate(pipeline_state::stopping);
    for (auto& ppl : this->_pipelines) {
        ppl.stop();
    }
    this->setstate(pipeline_state::stopped);
}

void
sbn::multi_pipeline::wait() {
    for (auto& ppl : this->_pipelines) {
        ppl.wait();
    }
}
