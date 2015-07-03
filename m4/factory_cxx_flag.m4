AC_DEFUN([FACTORY_CXX_FLAG],
[
	AC_MSG_CHECKING([$CXX for $1 flag])
	AC_LANG_PUSH([C++])
	ac_saved_cxxflags="$CXXFLAGS"
	CXXFLAGS="$1 -Werror"
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([],[])],
		[
			AC_MSG_RESULT([yes])
			CXXFLAGS="$ac_saved_cxxflags $1"
			AS_VAR_APPEND(MORE_CXXFLAGS, " $1")
		],
		[
			AC_MSG_RESULT([no])
		]
	)
	CXXFLAGS="$ac_saved_cxxflags"
	AC_SUBST([MORE_CXXFLAGS])
	AC_LANG_POP([C++])
])
