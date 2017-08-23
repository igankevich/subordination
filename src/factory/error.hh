#ifndef FACTORY_ERROR_HH
#define FACTORY_ERROR_HH

#include <ostream>
#include <stdexcept>
#include <unistd.h>
#include <unistdx/util/backtrace>

namespace factory {

	struct Error_location {

		inline
		Error_location(
			const char* file,
			const int line,
			const char* func
		) noexcept:
		_file(file),
		_line(line),
		_func(func)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Error_location& rhs);

	private:

		const char* _file;
		const int _line;
		const char* _func;

	};

	std::ostream&
	operator<<(std::ostream& out, const Error_location& rhs);

	struct Error: public std::runtime_error {

		inline
		Error(const std::string& msg, const Error_location& loc) noexcept:
		Error(msg.data(), loc)
		{}

		Error(const char* msg, const Error_location& loc) noexcept;

	protected:
		inline explicit
		Error(const Error_location& loc) noexcept:
		Error("error", loc)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Error& rhs);

	private:

		Error_location _location;

	};

	std::ostream&
	operator<<(std::ostream& out, const Error& rhs);

}

#define FACTORY_THROW(error, ...) \
	throw ::factory::error(__VA_ARGS__, {__FILE__,__LINE__,__func__})

#endif // FACTORY_ERROR_HH
