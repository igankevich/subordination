#ifndef FACTORY_KERNEL_KERNEL_HEADER_HH
#define FACTORY_KERNEL_KERNEL_HEADER_HH

#include <unistdx/net/endpoint>
#include <iosfwd>

namespace factory {

	class Kernel_header {

	public:
		typedef uint16_t app_type;

	private:
		sys::endpoint _src{};
		sys::endpoint _dst{};
		app_type _app = 0;

	public:
		Kernel_header() = default;
		Kernel_header(const Kernel_header&) = default;
		Kernel_header& operator=(const Kernel_header&) = default;
		~Kernel_header() = default;

		inline const sys::endpoint&
		from() const noexcept {
			return this->_src;
		}

		inline void
		from(const sys::endpoint& rhs) noexcept {
			this->_src = rhs;
		}

		inline const sys::endpoint&
		to() const noexcept {
			return this->_dst;
		}

		inline void
		to(const sys::endpoint& rhs) noexcept {
			this->_dst = rhs;
		}

		inline app_type
		app() const noexcept {
			return this->_app;
		}

		inline void
		setapp(app_type rhs) noexcept {
			this->_app = rhs;
		}

		inline bool
		is_foreign() const noexcept {
			return static_cast<bool>(this->_src);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Kernel_header& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const Kernel_header& rhs);

}

#endif // FACTORY_KERNEL_KERNEL_HEADER_HH vim:filetype=cpp
