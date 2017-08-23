#include "error.hh"
#include <thread>

factory::Error::Error(const char* msg, const Error_location& loc) noexcept:
std::runtime_error(msg),
_location(loc)
{ sys::backtrace(STDERR_FILENO); }

std::ostream&
factory::operator<<(std::ostream& out, const Error_location& rhs) {
	return out << rhs._file << ':' << rhs._line << ':' << rhs._func;
}

std::ostream&
factory::operator<<(std::ostream& out, const Error& rhs) {
	return out << rhs._location << ' '
		<< std::this_thread::get_id() << ' '
		<< rhs.what();
}

