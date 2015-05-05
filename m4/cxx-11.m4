AC_DEFUN([AX_CXX_11],
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
		CXXFLAGS="$CXXFLAGS -std=c++11"
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([#include <thread>],[std::thread t;])],
			[
				AC_MSG_RESULT([yes])
			],
			[
				AC_MSG_ERROR([no])
				CXXFLAGS="$ac_saved_cxxflags"
			]
		)
	fi
	AC_LANG_POP([C++])
])
