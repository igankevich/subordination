dnl @synopsis CHECK_CXX_11
dnl @summary check whether compiler supports C++11 standard
AC_DEFUN([CHECK_CXX_11],
[dnl
	AC_MSG_CHECKING([whether $CXX supports C++11 by default])
	AC_LANG_PUSH([C++])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([#include <thread>],[std::thread t;])],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])
		ac_cxx_11_ret=no]
	)
	if test x"$ac_cxx_11_ret" = xno; then
		AC_MSG_CHECKING([whether $CXX supports C++11 with -std=c++11])
		ac_saved_cxxflags="$CXXFLAGS"
		CXXFLAGS="-std=c++11"
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([#include <thread>],[std::thread t;])],
			[
				AC_MSG_RESULT([yes])
				AM_CXXFLAGS="$AM_CXXFLAGS -std=c++11"
			],
			[AC_MSG_ERROR([no])]
		)
		CXXFLAGS="$ac_saved_cxxflags"
		AC_SUBST([AM_CXXFLAGS])
	fi
	AC_LANG_POP([C++])
])
