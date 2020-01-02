#ifndef SUBORDINATION_PPL_MULTI_PIPELINE_HH
#define SUBORDINATION_PPL_MULTI_PIPELINE_HH

#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#include <vector>

namespace bsc {

    template <class T>
    class Multi_pipeline: public pipeline_base {

    public:
        typedef T kernel_type;
        typedef parallel_pipeline<T> base_pipeline;

    private:
        std::vector<base_pipeline> _pipelines;

    public:
        explicit
        Multi_pipeline(unsigned npipelines);
        Multi_pipeline(const Multi_pipeline&) = delete;
        Multi_pipeline(Multi_pipeline&&) = default;
        virtual ~Multi_pipeline() = default;

        inline base_pipeline&
        operator[](size_t i) noexcept {
            return this->_pipelines[i];
        }

        inline const base_pipeline&
        operator[](size_t i) const noexcept {
            return this->_pipelines[i];
        }

        inline size_t
        size() const noexcept {
            return this->_pipelines.size();
        }

        void
        set_name(const char* rhs);

        void
        start();

        void
        stop();

        void
        wait();
    };

}

#endif // vim:filetype=cpp
