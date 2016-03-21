#ifndef FACTORY_ERROR_HH
#define FACTORY_ERROR_HH

#include <stdexcept>
#include <thread>

namespace std {

	std::ostream&
	operator<<(std::ostream& out, const std::exception& rhs) {
		return out << rhs.what();
	}

}

namespace factory {

	namespace components {

		struct Error_location {

			Error_location(const char* file, const int line, const char* func) noexcept:
			_file(file),
			_line(line),
			_func(func)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Error_location& rhs) {
				return
				out << rhs._file << ':'
					<< rhs._line << ':'
					<< rhs._func;
			}

		private:

			const char* _file;
			const int _line;
			const char* _func;

		};

		struct Error: public std::runtime_error {

			Error(std::string&& msg, const char* file, const int line, const char* function) noexcept:
			std::runtime_error(std::forward<std::string>(msg)),
			_location{file, line, function}
			{}

			Error(const char* msg, const Error_location& loc) noexcept:
			std::runtime_error(msg),
			_location(loc)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Error& rhs) {
				return
				out << rhs._location << ' '
					<< std::this_thread::get_id() << ' '
					<< rhs.what();
			}

		private:

			Error_location _location;

		};

		struct Bad_kernel: public Error {

			typedef std::uintmax_t id_type;

			Bad_kernel(const char* msg, const Error_location& loc, id_type type_id) noexcept:
			Error(msg, loc),
			_id(type_id)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Bad_kernel& rhs) {
				operator<<(out, static_cast<const Error&>(rhs));
				return out << ' ' << "type_id" << '=' << rhs._id;
			}

		private:

			id_type _id;

		};

		struct Bad_type: public Error {

			typedef std::uintmax_t id_type;

			Bad_type(const char* msg, const Error_location& loc, id_type kernel_id) noexcept:
			Error(msg, loc),
			_id(kernel_id)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Bad_type& rhs) {
				operator<<(out, static_cast<const Error&>(rhs));
				return out << ' ' << "id" << '=' << rhs._id;
			}

		private:

			id_type _id;

		};

	}

}

#endif // FACTORY_ERROR_HH
