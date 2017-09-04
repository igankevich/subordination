#ifndef FACTORY_KERNEL_KERNEL_HEADER_HH
#define FACTORY_KERNEL_KERNEL_HEADER_HH

#include <unistdx/net/endpoint>
#include <factory/ppl/application.hh>
#include <iosfwd>

namespace factory {

	class kernel_header {

	private:
		sys::endpoint _src{};
		sys::endpoint _dst{};
		application_type _app = this_application::get_id();

	public:
		kernel_header() = default;
		kernel_header(const kernel_header&) = default;
		kernel_header& operator=(const kernel_header&) = default;
		~kernel_header() = default;

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

		inline application_type
		app() const noexcept {
			return this->_app;
		}

		inline void
		setapp(application_type rhs) noexcept {
			this->_app = rhs;
		}

		inline bool
		is_foreign() const noexcept {
			return !this->is_native();
		}

		inline bool
		is_native() const noexcept {
			return this->_app == this_application::get_id();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const kernel_header& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const kernel_header& rhs);

}

#endif // vim:filetype=cpp
