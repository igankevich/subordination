#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Factory], [0.1], [igankevich@yandex.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/test_buffer.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(ssl, SSL_library_init)
AC_CHECK_LIB(crypto, OPENSSL_add_all_algorithms_noconf)
AC_CHECK_LIB(resolv, __b64_ntop)

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

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([clock_gettime memchr memset socket])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
