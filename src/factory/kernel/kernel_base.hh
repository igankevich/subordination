#ifndef FACTORY_KERNEL_KERNEL_BASE_HH
#define FACTORY_KERNEL_KERNEL_BASE_HH

#include <bitset>
#include <cassert>
#include <chrono>

#include <factory/kernel/exit_code.hh>
#include <factory/kernel/kernel_flag.hh>

namespace asc {

	class kernel_base {

	public:
		typedef std::chrono::system_clock clock_type;
		typedef clock_type::time_point time_point;
		typedef clock_type::duration duration;
		typedef std::bitset<6> flags_type;

	protected:
		exit_code _result = exit_code::undefined;
		time_point _at = time_point(duration::zero());
		flags_type _flags = 0;

	public:
		virtual
		~kernel_base() {
			assert(!this->isset(kernel_flag::deleted));
			this->setf(kernel_flag::deleted);
		}

		inline exit_code
		return_code() const noexcept {
			return _result;
		}

		inline void
		return_code(exit_code rhs) noexcept {
			this->_result = rhs;
		}

		// scheduled kernels
		inline time_point
		at() const noexcept {
			return this->_at;
		}

		inline void
		at(time_point t) noexcept {
			this->_at = t;
		}

		inline void
		after(duration delay) noexcept {
			this->_at = clock_type::now() + delay;
		}

		inline bool
		scheduled() const noexcept {
			return this->_at != time_point(duration::zero());
		}

		// flags
		inline flags_type
		flags() const noexcept {
			return this->_flags;
		}

		inline void
		setf(kernel_flag f) noexcept {
			this->_flags.set(static_cast<size_t>(f));
		}

		inline void
		unsetf(kernel_flag f) noexcept {
			this->_flags.reset(static_cast<size_t>(f));
		}

		inline bool
		isset(kernel_flag f) const noexcept {
			return this->_flags.test(static_cast<size_t>(f));
		}

		inline bool
		carries_parent() const noexcept {
			return this->isset(kernel_flag::carries_parent);
		}

		#ifdef SPRINGY
		float _mass = 1.f;
		#endif

	};

}

#endif // vim:filetype=cpp
