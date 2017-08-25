#ifndef FACTORY_PPL_BASIC_CPU_SERVER_HH
#define FACTORY_PPL_BASIC_CPU_SERVER_HH

#include "basic_server.hh"

namespace factory {

	template<class T>
	class Basic_CPU_server: public Basic_server<T> {

	public:
		typedef Basic_server<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;
		using typename base_server::traits_type;

		inline
		Basic_CPU_server(Basic_CPU_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		inline
		Basic_CPU_server() noexcept:
		Basic_CPU_server(1u)
		{}

		inline explicit
		Basic_CPU_server(unsigned concurrency) noexcept:
		base_server(concurrency)
		{}

		Basic_CPU_server(const Basic_CPU_server&) = delete;
		Basic_CPU_server& operator=(const Basic_CPU_server&) = delete;
		~Basic_CPU_server() = default;

	protected:

		void
		do_run() override;

	};

}

#endif // FACTORY_PPL_BASIC_CPU_SERVER_HH vim:filetype=cpp
