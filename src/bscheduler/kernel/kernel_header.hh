#ifndef BSCHEDULER_KERNEL_KERNEL_HEADER_HH
#define BSCHEDULER_KERNEL_KERNEL_HEADER_HH

#include <iosfwd>
#include <memory>

#include <unistdx/net/endpoint>

#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/kernel_header_flag.hh>

namespace bsc {

	class kernel_header {

	public:
		typedef std::unique_ptr<application> application_ptr;
		typedef kernel_header_flag flag_type;

	private:
		flag_type _flags = flag_type(0);
		sys::endpoint _src{};
		sys::endpoint _dst{};
		application_type _aid = this_application::get_id();
		const application* _aptr = nullptr;

	public:
		kernel_header() = default;
		kernel_header(const kernel_header&) = default;
		kernel_header& operator=(const kernel_header&) = default;

		inline
		~kernel_header() {
			if (this->owns_application()) {
				delete this->_aptr;
			}
		}

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
			return this->_aid;
		}

		inline void
		setapp(application_type rhs) noexcept {
			this->_aid = rhs;
		}

		inline bool
		is_foreign() const noexcept {
			return !this->is_native();
		}

		inline bool
		is_native() const noexcept {
			return this->_aid == this_application::get_id();
		}

		inline const application*
		aptr() const noexcept {
			return this->_aptr;
		}

		inline void
		aptr(const application* rhs) noexcept {
			if (this->owns_application()) {
				delete this->_aptr;
			}
			if (rhs) {
				this->_flags |= flag_type::has_application;
				this->_aid = rhs->id();
			} else {
				this->_flags &= ~flag_type::has_application;
			}
			this->_aptr = rhs;
		}

		inline bool
		has_application() const noexcept {
			return this->_flags & kernel_header_flag::has_application;
		}

		inline bool
		owns_application() const noexcept {
			return this->_flags & flag_type::owns_application;
		}

		inline bool
		has_source_and_destination() const noexcept {
			return this->_flags & kernel_header_flag::has_source_and_destination;
		}

		inline void
		prepend_source_and_destination() {
			this->_flags |= kernel_header_flag::has_source_and_destination;
		}

		inline void
		do_not_prepend_source_and_destination() {
			this->_flags &= ~kernel_header_flag::has_source_and_destination;
		}

		void
		write_header(sys::pstream& out) const;

		void
		read_header(sys::pstream& in);

		friend std::ostream&
		operator<<(std::ostream& out, const kernel_header& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const kernel_header& rhs);

	inline sys::pstream&
	operator<<(sys::pstream& out, const kernel_header& rhs) {
		rhs.write_header(out);
		return out;
	}

	inline sys::pstream&
	operator>>(sys::pstream& in, kernel_header& rhs) {
		rhs.read_header(in);
		return in;
	}

}

#endif // vim:filetype=cpp
