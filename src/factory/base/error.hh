#ifndef FACTORY_BASE_ERROR_HH
#define FACTORY_BASE_ERROR_HH

#include <iosfwd>
#include <stdexcept>

namespace factory {

	class Error: public std::runtime_error {

	private:
		const char* _file = "<unknown>";
		int _line = 0;
		const char* _func = "<unknown>";

	public:
		inline explicit
		Error(const char* msg) noexcept:
		std::runtime_error(msg)
		{}

		virtual ~Error() noexcept = default;

		inline const Error&
		set_location(const char* file, int line, const char* func) noexcept {
			this->_file = file;
			this->_line = line;
			this->_func = func;
			return *this;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Error& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const Error& rhs);

}

#define FACTORY_THROW(error, ...) \
	throw ::factory::error(__VA_ARGS__).set_location(__FILE__,__LINE__,__func__)

#endif // vim:filetype=cpp
