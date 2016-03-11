#ifndef SYSX_BITS_MACROS_HH
#define SYSX_BITS_MACROS_HH

#define SYSX_GCC_VERSION_AT_LEAST(major, minor) \
	((__GNUC__ > major) || (__GNUC__ == major && __GNUC_MINOR__ >= minor))

#endif // SYSX_BITS_MACROS_HH
