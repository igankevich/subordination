#ifndef FACTORY_PPL_CPU_SERVER_HH
#define FACTORY_PPL_CPU_SERVER_HH

#include "basic_pipeline.hh"

namespace factory {

	template<class T>
	class parallel_pipeline: public basic_pipeline<T> {

	public:
		typedef basic_pipeline<T> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::traits_type;

		inline
		parallel_pipeline(parallel_pipeline&& rhs) noexcept:
		base_pipeline(std::move(rhs))
		{}

		inline
		parallel_pipeline() noexcept:
		parallel_pipeline(1u)
		{}

		inline explicit
		parallel_pipeline(unsigned concurrency) noexcept:
		base_pipeline(concurrency)
		{}

		parallel_pipeline(const parallel_pipeline&) = delete;
		parallel_pipeline& operator=(const parallel_pipeline&) = delete;
		~parallel_pipeline() = default;

	protected:

		void
		do_run() override;

	};

}

#endif // vim:filetype=cpp
