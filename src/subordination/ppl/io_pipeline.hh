#ifndef SUBORDINATION_PPL_IO_PIPELINE_HH
#define SUBORDINATION_PPL_IO_PIPELINE_HH

#include <subordination/ppl/parallel_pipeline.hh>

namespace sbn {

    class io_pipeline: public parallel_pipeline {

    public:

        inline
        io_pipeline(io_pipeline&& rhs) noexcept:
        parallel_pipeline(std::move(rhs))
        {}

        /// use only one thread to read/write data
        inline
        io_pipeline() noexcept:
        io_pipeline(sys::io_concurrency()*2u)
        {}

        inline explicit
        io_pipeline(unsigned concurrency) noexcept:
        parallel_pipeline(concurrency)
        {}

        io_pipeline(const io_pipeline&) = delete;
        io_pipeline& operator=(const io_pipeline&) = delete;
        io_pipeline& operator=(io_pipeline&&) = delete;
        ~io_pipeline() = default;

    };

}

#endif // vim:filetype=cpp
