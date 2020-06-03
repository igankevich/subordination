#ifndef SUBORDINATION_PPL_PARALLEL_PIPELINE_HH
#define SUBORDINATION_PPL_PARALLEL_PIPELINE_HH

#include <subordination/ppl/basic_pipeline.hh>

namespace sbn {

    class parallel_pipeline: public basic_pipeline<> {

    public:
        using typename basic_pipeline::lock_type;
        using typename basic_pipeline::traits_type;

        inline
        parallel_pipeline(parallel_pipeline&& rhs) noexcept:
        basic_pipeline(std::move(rhs))
        {}

        inline
        parallel_pipeline() noexcept:
        parallel_pipeline(1u)
        {}

        inline explicit
        parallel_pipeline(unsigned concurrency) noexcept:
        basic_pipeline(concurrency)
        {}

        parallel_pipeline(const parallel_pipeline&) = delete;
        parallel_pipeline& operator=(const parallel_pipeline&) = delete;
        ~parallel_pipeline() = default;

    protected:

        void do_run() override;

    };

}

#endif // vim:filetype=cpp
