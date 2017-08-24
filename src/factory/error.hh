#ifndef FACTORY_ERROR_HH
#define FACTORY_ERROR_HH

#include <iosfwd>
#include <stdexcept>

namespace factory {

	struct Error_location {

		Error_location() = default;

		inline
		Error_location(const char* file, int line, const char* func) noexcept:
		_file(file),
		_line(line),
		_func(func)
		{}

		Error_location&
		operator=(const Error_location&) = default;

		friend std::ostream&
		operator<<(std::ostream& out, const Error_location& rhs);

	private:

		const char* _file = "<unknown>";
		int _line = 0;
		const char* _func = "<unknown>";

	};

	std::ostream&
	operator<<(std::ostream& out, const Error_location& rhs);

	struct Error: public std::runtime_error {

		inline explicit
		Error(const char* msg) noexcept:
		std::runtime_error(msg)
		{}

		inline const Error&
		set_location(const Error_location& rhs) noexcept {
			this->_location = rhs;
			return *this;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Error& rhs);

	private:

		Error_location _location;

	};

	std::ostream&
	operator<<(std::ostream& out, const Error& rhs);

}

#define FACTORY_THROW(error, ...) \
	throw ::factory::error(__VA_ARGS__).set_location({__FILE__,__LINE__,__func__})

#endif // FACTORY_ERROR_HH
