#ifndef FACTORY_KERNEL_BASIC_KERNEL_HH
#define FACTORY_KERNEL_BASIC_KERNEL_HH

#include <bitset>
#include <chrono>

#include "result.hh"

namespace factory {

	class Basic_kernel {

	public:
		typedef std::chrono::system_clock Clock;
		typedef Clock::time_point Time_point;
		typedef Clock::duration Duration;
		typedef std::bitset<3> Flags;

		/// Various kernel flags.
		enum struct Flag { DELETED = 0,
			/**
			Setting the flag tells \link factory::Kernel_stream\endlink that
			the kernel carries a backup copy of its parent to the subordinate
			node. This is the main mechanism of providing fault tolerance for
			principal kernels, it works only when <em>principal kernel have
			only one subordinate at a time</em>.
			*/
			carries_parent = 1,
			/**
			\deprecated This flag should not be used for purposes other than
			debugging.
			*/
			priority_service = 2 };

	protected:
		Result _result = Result::undefined;
		Time_point _at = Time_point(Duration::zero());
		Flags _flags = 0;

	public:
		virtual
		~Basic_kernel() = default;

		inline Result
		result() const noexcept {
			return _result;
		}

		inline void
		result(Result rhs) noexcept {
			this->_result = rhs;
		}

		// scheduled kernels
		inline Time_point
		at() const noexcept {
			return this->_at;
		}

		inline void
		at(Time_point t) noexcept {
			this->_at = t;
		}

		inline void
		after(Duration delay) noexcept {
			this->_at = Clock::now() + delay;
		}

		inline bool
		scheduled() const noexcept {
			return this->_at != Time_point(Duration::zero());
		}

		// flags
		inline Flags
		flags() const noexcept {
			return this->_flags;
		}

		inline void
		setf(Flag f) noexcept {
			this->_flags.set(static_cast<size_t>(f));
		}

		inline void
		unsetf(Flag f) noexcept {
			this->_flags.reset(static_cast<size_t>(f));
		}

		inline bool
		isset(Flag f) const noexcept {
			return this->_flags.test(static_cast<size_t>(f));
		}

		inline bool
		carries_parent() const noexcept {
			return this->isset(Flag::carries_parent);
		}

		#ifdef SPRINGY
		float _mass = 1.f;
		#endif

	};

}

#endif // FACTORY_KERNEL_BASIC_KERNEL_HH vim:filetype=cpp
