#ifndef SYS_PATH_HH
#define SYS_PATH_HH

#include <limits.h>
#include <stdlib.h>

#include <string>
#include <memory>

namespace sys {

	struct path;

	struct const_path {

		constexpr
		const_path(const char* rhs) noexcept:
		_str(rhs)
		{}

		const_path(const path&) noexcept;

		operator const char*() const noexcept {
			return _str;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const const_path& rhs) {
			return out << rhs._str;
		}

	private:

		const char* _str;

	};

	struct path {

		static const char separator = '/';

		path() = default;

		explicit
		path(const_path rhs):
		_path(rhs)
		{}

		explicit
		path(std::string&& rhs):
		_path(std::forward<std::string>(rhs))
		{}

		path(const path& rhs):
		_path(rhs._path)
		{}

		path(path&& rhs):
		_path(std::forward<std::string>(rhs._path))
		{}

		template<class A, class B>
		path(A&& dir, B&& filename):
		_path(std::forward<A>(dir))
		{
			_path.push_back(separator);
			_path.append(std::forward<B>(filename));
		}

		operator const char* () const noexcept {
			return _path.data();
		}

		path&
		operator=(const path&) = default;

		friend class const_path;

		friend std::ostream&
		operator<<(std::ostream& out, const path& rhs) {
			return out << rhs._path;
		}

	private:

		std::string _path;

	};

	const_path::const_path(const path& rhs) noexcept:
	_str(rhs._path.data())
	{}

	struct canonical_path: public path {

		canonical_path(path&& rhs):
		path(canonicalise(std::forward<path>(rhs)))
		{}

		template<class A, class B>
		canonical_path(A&& dir, B&& filename):
		path(canonicalise(path(std::forward<A>(dir), std::forward<B>(filename))))
		{}

		canonical_path(canonical_path&& rhs):
		path(std::forward<path>(rhs))
		{}

		static path
		canonicalise(path&& rhs) {
			std::unique_ptr<char> ptr(bits::check<char*>(
				::realpath(const_path(rhs), nullptr),
				static_cast<char*>(nullptr),
				__FILE__, __LINE__, __func__
			));
			return path(const_path(ptr.get()));
		}

	};

}

#endif // SYS_PATH_HH
