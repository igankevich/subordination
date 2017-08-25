#ifndef FACTORY_IO_SERVER_HH
#define FACTORY_IO_SERVER_HH

#include "cpu_server.hh"

namespace factory {

	template<class T>
	struct IO_server: public Basic_CPU_server<T> {

		typedef Basic_CPU_server<T> base_server;
		using typename base_server::kernel_type;
		using typename base_server::lock_type;

		inline
		IO_server(IO_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		/// use only one thread to read/write data
		inline
		IO_server() noexcept:
		IO_server(sys::io_concurrency() * 2u)
		{}

		inline explicit
		IO_server(unsigned concurrency) noexcept:
		base_server(concurrency)
		{}

		IO_server(const IO_server&) = delete;
		IO_server& operator=(const IO_server&) = delete;
		~IO_server() = default;

	};

}

#endif // FACTORY_IO_SERVER_HH
