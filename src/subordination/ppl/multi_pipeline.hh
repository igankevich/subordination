#ifndef SUBORDINATION_PPL_MULTI_PIPELINE_HH
#define SUBORDINATION_PPL_MULTI_PIPELINE_HH

#include <vector>

#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>

namespace sbn {

    class multi_pipeline: public pipeline_base {

    private:
        std::vector<parallel_pipeline> _pipelines;

    public:
        explicit multi_pipeline(unsigned npipelines);
        multi_pipeline(const multi_pipeline&) = delete;
        multi_pipeline(multi_pipeline&&) = default;
        virtual ~multi_pipeline() = default;

        inline parallel_pipeline&
        operator[](size_t i) noexcept {
            return this->_pipelines[i];
        }

        inline const parallel_pipeline&
        operator[](size_t i) const noexcept {
            return this->_pipelines[i];
        }

        inline size_t size() const noexcept { return this->_pipelines.size(); }

        void set_name(const char* rhs);
        void start();
        void stop();
        void wait();

    };

}

#endif // vim:filetype=cpp
