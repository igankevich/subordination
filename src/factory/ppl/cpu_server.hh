#ifndef FACTORY_PPL_BASIC_CPU_SERVER_HH
#define FACTORY_PPL_BASIC_CPU_SERVER_HH

#include "basic_server.hh"

namespace factory {

	template<class T>
	class CPU_server: public Basic_server<T> {

	public:
		typedef Basic_server<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;
		using typename base_server::traits_type;

		inline
		CPU_server(CPU_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		inline
		CPU_server() noexcept:
		CPU_server(1u)
		{}

		inline explicit
		CPU_server(unsigned concurrency) noexcept:
		base_server(concurrency)
		{}

		CPU_server(const CPU_server&) = delete;
		CPU_server& operator=(const CPU_server&) = delete;
		~CPU_server() = default;

	protected:

		void
		do_run() override;

	};

}

#endif // FACTORY_PPL_BASIC_CPU_SERVER_HH vim:filetype=cpp
