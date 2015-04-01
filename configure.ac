#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Factory], [0.1], [igankevich@yandex.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/test_buffer.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_LANG([C++])
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
#AC_CHECK_LIB(ssl, SSL_library_init, [],
#	AC_MSG_ERROR([Could not find SSL library]))
#AC_CHECK_LIB(crypto, OPENSSL_add_all_algorithms_noconf, [],
#	AC_MSG_ERROR([Could not find crypto library]))
AC_CHECK_LIB(resolv, __b64_ntop, [],
	AC_MSG_ERROR([Could not find resolv library]))

# Checks for header files.
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h fenv.h netdb.h netinet/in.h stdlib.h strings.h sys/socket.h unistd.h])

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

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_CHECK_FUNCS([memchr memset socket setenv strerror])
AX_PTHREAD

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
