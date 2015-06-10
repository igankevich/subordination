AC_PREREQ([2.69])
AC_INIT([Factory], [0.1], [Ivan Gankevich <igankevich@yandex.com>])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/test_buffer.cc])
AC_CONFIG_HEADERS([config.h])

# Automake initialisation.
AM_INIT_AUTOMAKE([-Wall])
#AM_SILENT_RULES([yes])

# Check for arguments.
AC_ARG_ENABLE([warnings],
	AS_HELP_STRING([--enable-warnings], [enable all known compiler warnings]))
AC_ARG_ENABLE([perftests],
	AS_HELP_STRING([--enable-perf-tests], [perform tests to choose the most efficient routines for locking, polling etc.]))

# Checks for programs.
AC_LANG([C++])
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])
AC_TYPE_PID_T
AX_CXX_11
AX_CXX_11_THREAD
AX_STACK_TRACE
AX_GCC_BUILTIN(__builtin_bswap64)
AX_GCC_BUILTIN(__builtin_bswap32)

AS_IF([test "x$enable_warnings" = "xyes"], [AX_CXX_WARNINGS])

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([socket setenv strerror])

# Code coverage, memory leaks and threading errors automated tests.
AX_CODE_COVERAGE
AX_VALGRIND_CHECK

# Documentation.
DX_DOXYGEN_FEATURE(ON)
DX_DOT_FEATURE(OFF)
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(factory)

# Clean ALL files
AX_CLEAN_ALL

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
