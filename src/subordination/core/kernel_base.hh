#ifndef SUBORDINATION_CORE_KERNEL_BASE_HH
#define SUBORDINATION_CORE_KERNEL_BASE_HH

#include <cassert>
#include <chrono>
#include <iosfwd>

#include <unistdx/base/flag>
#include <unistdx/base/types>

namespace sbn {

    enum class exit_code: sys::u16 {
        success = 0,
        undefined = 1,
        error = 2,
        endpoint_not_connected = 3,
        no_principal_found = 4,
        no_upstream_servers_available = 5,
        no_resources = 6,
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
        /** When a kernel is transactional, it is rolled back even if power
        failure occures in the middle of its \link sbn::kernel::act \endlink
        method. */
        transactional = 1<<4,
        /** Send the kernel to the superior of cluster node hierarchy. */
        send_to_superior_node = 1<<5,
        /** Send the kernel to all of the subordinates of cluster node hierarchy. */
        send_to_subordinate_node = 1<<6,
        /** Allocate a separate thread to execute the kernel in parallel pipeline. */
        new_thread = 1<<7,
    };

    UNISTDX_FLAGS(kernel_flag)

    class kernel_base {

    public:
        using clock_type = std::chrono::system_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;
        using flag = kernel_flag;

    protected:
        exit_code _result = exit_code::undefined;
        time_point _at{};
        kernel_flag _flags{};

    public:
        kernel_base() = default;
        virtual ~kernel_base() = default;
        kernel_base(const kernel_base&) = default;
        kernel_base& operator=(const kernel_base&) = default;
        kernel_base(kernel_base&&) = default;
        kernel_base& operator=(kernel_base&&) = default;

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

        inline bool new_thread() const noexcept {
            return isset(kernel_flag::new_thread);
        }

        // flags
        inline kernel_flag flags() const noexcept { return this->_flags; }
        inline void flags(kernel_flag rhs) noexcept { this->_flags = rhs; }
        inline void setf(kernel_flag f) noexcept { this->_flags |= f; }
        inline void unsetf(kernel_flag f) noexcept { this->_flags &= ~f; }
        inline bool isset(kernel_flag f) const noexcept { return bool(this->_flags & f); }

        inline bool
        carries_parent() const noexcept {
            return this->isset(kernel_flag::carries_parent);
        }

    };

}

#endif // vim:filetype=cpp
