#include <subordination/ppl/multi_pipeline.hh>

template <class T>
sbn::Multi_pipeline<T>::Multi_pipeline(unsigned npipelines):
_pipelines(npipelines) {
    unsigned num = 0;
    for (base_pipeline& ppl : this->_pipelines) {
        ppl.set_number(num);
        ++num;
    }
}

template <class T>
void
sbn::Multi_pipeline<T>::set_name(const char* rhs) {
    for (base_pipeline& ppl : this->_pipelines) {
        ppl.set_name(rhs);
    }
}


template <class T>
void
sbn::Multi_pipeline<T>::start() {
    this->setstate(pipeline_state::starting);
    for (base_pipeline& ppl : this->_pipelines) {
        ppl.start();
    }
    this->setstate(pipeline_state::started);
}

template <class T>
void
sbn::Multi_pipeline<T>::stop() {
    this->setstate(pipeline_state::stopping);
    for (base_pipeline& ppl : this->_pipelines) {
        ppl.stop();
    }
    this->setstate(pipeline_state::stopped);
}

template <class T>
void
sbn::Multi_pipeline<T>::wait() {
    for (base_pipeline& ppl : this->_pipelines) {
        ppl.wait();
    }
}

template class sbn::Multi_pipeline<sbn::kernel>;
