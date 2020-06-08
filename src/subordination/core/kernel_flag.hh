#ifndef SUBORDINATION_CORE_KERNEL_FLAG_HH
#define SUBORDINATION_CORE_KERNEL_FLAG_HH

#include <unistdx/base/types>

#include <subordination/core/macros.hh>

namespace sbn {

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

}

#endif // vim:filetype=cpp
