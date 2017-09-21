#ifndef BSCHEDULER_BASE_ERROR_HH
#define BSCHEDULER_BASE_ERROR_HH

#include <iosfwd>
#include <stdexcept>

namespace bsc {

	class error: public std::runtime_error {

	private:
		const char* _file = "<unknown>";
		int _line = 0;
		const char* _func = "<unknown>";

	public:
		inline explicit
		error(const char* msg) noexcept:
		std::runtime_error(msg)
		{}

		virtual ~error() noexcept = default;

		inline const error&
		set_location(const char* file, int line, const char* func) noexcept {
			this->_file = file;
			this->_line = line;
			this->_func = func;
			return *this;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const error& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const error& rhs);

}

#define BSCHEDULER_THROW(error, ...) \
	throw ::bsc::error(__VA_ARGS__).set_location(__FILE__,__LINE__,__func__)

#endif // vim:filetype=cpp
