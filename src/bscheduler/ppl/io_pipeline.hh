#ifndef BSCHEDULER_PPL_IO_PIPELINE_HH
#define BSCHEDULER_PPL_IO_PIPELINE_HH

#include "parallel_pipeline.hh"

namespace bsc {

	template<class T>
	struct io_pipeline: public parallel_pipeline<T> {

		typedef parallel_pipeline<T> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::lock_type;

		inline
		io_pipeline(io_pipeline&& rhs) noexcept:
		base_pipeline(std::move(rhs))
		{}

		/// use only one thread to read/write data
		inline
		io_pipeline() noexcept:
		io_pipeline(sys::io_concurrency()*2u)
		{}

		inline explicit
		io_pipeline(unsigned concurrency) noexcept:
		base_pipeline(concurrency)
		{}

		io_pipeline(const io_pipeline&) = delete;

		io_pipeline&
		operator=(const io_pipeline&) = delete;

		~io_pipeline() = default;

	};

}

#endif // vim:filetype=cpp
