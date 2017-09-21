#ifndef FACTORY_KERNEL_KERNEL_FLAG_HH
#define FACTORY_KERNEL_KERNEL_FLAG_HH

namespace asc {

	/// Various kernel flags.
	enum struct kernel_flag {
		deleted = 0,
		/**
		   Setting the flag tells \link asc::kstream\endlink that
		   the kernel carries a backup copy of its parent to the subordinate
		   node. This is the main mechanism of providing fault tolerance for
		   principal kernels, it works only when <em>principal kernel have
		   only one subordinate at a time</em>.
		 */
		carries_parent = 1,
		parent_is_id = 3,
		principal_is_id = 4,
		do_not_delete = 5
	};

}

#endif // vim:filetype=cpp
