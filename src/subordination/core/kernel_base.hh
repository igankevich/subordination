#ifndef SUBORDINATION_CORE_KERNEL_BASE_HH
#define SUBORDINATION_CORE_KERNEL_BASE_HH

#include <bitset>
#include <cassert>
#include <chrono>
#include <iosfwd>

#if defined(SBN_DEBUG)
#include <unistdx/util/backtrace>
#endif

#include <unistdx/base/types>

#include <subordination/core/macros.hh>

namespace sbn {

    enum class exit_code: sys::u16 {
        success = 0,
        undefined = 1,
        error = 2,
        endpoint_not_connected = 3,
        no_principal_found = 4,
        no_upstream_servers_available = 5
    };

    const char* to_string(exit_code rhs) noexcept;
    std::ostream& operator<<(std::ostream& out, exit_code rhs);

    /// Various kernel flags.
    enum class kernel_flag: sys::u32 {
        deleted = 1<<0,
        /**
           Setting the flag tells \link sbn::kernel_buffer \endlink that
           the kernel carries a backup copy of its parent to the subordinate
           node. This is the main mechanism of providing fault tolerance for
           principal kernels, it works only when <em>principal kernel have
           only one subordinate at a time</em>.
         */
        carries_parent = 1<<1,
        parent_is_id = 1<<2,
        principal_is_id = 1<<3,
        do_not_delete = 1<<4,
    };

    SBN_FLAGS(kernel_flag)

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

        inline exit_code return_code() const noexcept { return this->_result; }
        inline void return_code(exit_code rhs) noexcept { this->_result = rhs; }

        // scheduled kernels
        inline time_point at() const noexcept { return this->_at; }
        inline void at(time_point t) noexcept { this->_at = t; }
        inline void after(duration delay) noexcept { this->_at = clock_type::now() + delay; }

        inline bool
        scheduled() const noexcept {
            return this->_at != time_point(duration::zero());
        }

        // flags
        inline kernel_flag flags() const noexcept { return this->_flags; }
        inline void flags(kernel_flag rhs) noexcept { this->_flags = rhs; }
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
