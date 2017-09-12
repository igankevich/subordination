#ifndef FACTORY_KERNEL_KERNEL_FLAG_HH
#define FACTORY_KERNEL_KERNEL_FLAG_HH

namespace factory {

	/// Various kernel flags.
	enum struct kernel_flag {
		DELETED = 0,
		/**
		   Setting the flag tells \link factory::kstream\endlink that
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
		priority_service = 2,
		parent_is_id = 3,
		principal_is_id = 4,
	};

}

#endif // vim:filetype=cpp
