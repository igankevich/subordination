#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Factory], [0.1], [igankevich@yandex.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/test_buffer.cc])
AC_CONFIG_HEADERS([config.h])

# Automake initialisation.
AM_INIT_AUTOMAKE
#AM_SILENT_RULES([yes])

# Check for arguments.
AC_ARG_ENABLE([warnings],
	AS_HELP_STRING([--enable-warnings], [enable all known compiler warnings]))
AC_ARG_ENABLE([ssl],
	AS_HELP_STRING([--enable-ssl], [enable SSL library]))

# Checks for programs.
AC_LANG([C++])
AC_PROG_CXX

# Checks for libraries.
AS_IF([test "x$enable_ssl" = "xyes"], [
	AC_CHECK_LIB(ssl, SSL_library_init, [],
		AC_MSG_ERROR([Could not find SSL library]))
	AC_CHECK_LIB(crypto, OPENSSL_add_all_algorithms_noconf, [],
		AC_MSG_ERROR([Could not find crypto library]))
])
#AC_CHECK_LIB(ssl, SSL_library_init, [],
#	AC_MSG_ERROR([Could not find SSL library]))
#AC_CHECK_LIB(crypto, OPENSSL_add_all_algorithms_noconf, [],
#	AC_MSG_ERROR([Could not find crypto library]))
#AC_CHECK_LIB(resolv, __b64_ntop, [],
#	AC_MSG_ERROR([Could not find resolv library]))

# Checks for header files.
#AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_PID_T
CHECK_CXX_11
CHECK_CXX_11_THREAD

AS_IF([test "x$enable_warnings" = "xyes"], [AX_CXX_WARNINGS])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_CHECK_FUNCS([memset socket setenv strerror])

# Clean ALL files
AX_CLEAN_ALL

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
