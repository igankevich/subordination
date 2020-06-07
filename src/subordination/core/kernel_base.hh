#ifndef SUBORDINATION_CORE_KERNEL_BASE_HH
#define SUBORDINATION_CORE_KERNEL_BASE_HH

#include <bitset>
#include <cassert>
#include <chrono>

#if defined(SBN_DEBUG)
#include <unistdx/util/backtrace>
#endif

#include <subordination/core/exit_code.hh>
#include <subordination/core/kernel_flag.hh>

namespace sbn {

    class kernel_base {

    public:
        using clock_type = std::chrono::system_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;
        using flags_type = kernel_flag;

    protected:
        exit_code _result = exit_code::undefined;
        time_point _at = time_point(duration::zero());
        kernel_flag _flags{};

    public:
        virtual
        ~kernel_base() {
            #if defined(SBN_DEBUG)
            if (this->isset(kernel_flag::deleted)) {
                sys::backtrace(2);
            }
            assert(!this->isset(kernel_flag::deleted));
            #endif
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
        inline kernel_flag flags() const noexcept { return this->_flags; }
        inline void setf(kernel_flag f) noexcept { this->_flags = this->_flags | f; }
        inline void unsetf(kernel_flag f) noexcept { this->_flags = this->_flags & ~f; }
        inline bool isset(kernel_flag f) const noexcept { return bool(this->_flags & f); }

        inline bool
        carries_parent() const noexcept {
            return this->isset(kernel_flag::carries_parent);
        }

    };

}

#endif // vim:filetype=cpp
