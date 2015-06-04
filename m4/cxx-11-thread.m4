dnl @synopsis CHECK_CXX_11_THREAD
dnl @summary check whether compiler supports C++11 threads
AC_DEFUN([AX_CXX_NEEDS_FLAG],
[dnl
	ax_var=$4
	AC_LANG_PUSH([C++])
	for f in "" $3; do
		AS_IF([test "x$f" = x],[ax_msg="without options"],[ax_msg="with $f"])
		AC_MSG_CHECKING([$1 $ax_msg])
		ax_saved_cxxflags="$CXXFLAGS"
		ax_saved_ldflags="$LDFLAGS"
		AS_VAR_APPEND($ax_var, " $f")
		AC_RUN_IFELSE(
			[$2],
			[
				AC_MSG_RESULT([yes])
				ax_cxx_needs_flag_ok=yes
			],
			[
				AC_MSG_RESULT([no])
				ax_cxx_needs_flag_ok=no
				CXXFLAGS="$ax_saved_cxxflags"
				LDFLAGS="$ax_saved_ldflags"
			]
		)
		if test x"$ax_cxx_needs_flag_ok" = xyes; then
			break
		fi
	done
	AC_LANG_POP([C++])
])




dnl @synopsis CHECK_CXX_11_THREAD
dnl @summary check whether compiler supports C++11 threads
AC_DEFUN([AX_CXX_11_THREAD],
[dnl
	AX_CXX_NEEDS_FLAG(
		[whether $CXX supports C++11 threads],
		[AC_LANG_PROGRAM([
			#include <thread>
			void main_func() {}
		],[
			std::thread t(main_func);
			t.join();
		])],
		[-lpthread],
		[LDFLAGS]
	)
	AX_CXX_NEEDS_FLAG(
		[whether $CXX supports C++11 threads],
		[AC_LANG_PROGRAM([
			#include <thread>
			void main_func() {}
		],[
			std::thread t(main_func);
			t.join();
		])],
		[-pthread -pthreads],
		[CXXFLAGS]
	)
])
