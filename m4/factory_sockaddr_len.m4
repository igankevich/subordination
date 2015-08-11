AC_DEFUN([FACTORY_SOCKADDR_LEN], [
	AC_MSG_CHECKING([whether struct sockaddr_in contains sin_len])
	AC_CACHE_VAL([factory_cv_sockaddr_len], [
		AC_COMPILE_IFELSE([
			AC_LANG_PROGRAM([
				#include <netinet/in.h>
				],[
				struct ::sockaddr_in addr;
				addr.sin_len = 10;
				return 0;
			])],
			[factory_cv_sockaddr_len=yes],
			[factory_cv_sockaddr_len=no]
		)
	])
	AS_IF([test "x$factory_cv_sockaddr_len" = xyes],
		[
    		AC_MSG_RESULT([yes])
			AC_DEFINE(HAVE_SOCKADDR_LEN,1,
	            [define if there is sin_len field in struct sockaddr_in])
			$1
		],
		[
    		AC_MSG_RESULT([yes])
			$2
	])
])
