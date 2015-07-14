AC_DEFUN([FACTORY_CHECK_CODE], [
	AC_ARG_ENABLE([cppcheck],
	              [AS_HELP_STRING([--enable-cppcheck], [Whether to enable Cppcheck on the unit tests])],
	              [enable_cppcheck=$enableval],[enable_cppcheck=no])
	AC_CHECK_PROG([CPPCHECK],[cppcheck],[cppcheck])
	AC_MSG_CHECKING([whether to enable Cppcheck on the unit tests])
	AS_IF([test "$enable_cppcheck" = "yes" -a "$CPPCHECK" = ""],[
		AC_MSG_ERROR([Could not find cppcheck; either install it or reconfigure with --disable-cppcheck])
	])
	AM_CONDITIONAL([CPPCHECK_ENABLED],[test "$enable_cppcheck" = "yes"])
	AC_SUBST([CPPCHECK_ENABLED],[$enable_cppcheck])
	AC_MSG_RESULT([$enable_cppcheck])
CPPCHECK_RULES='
CPPCHECK_MANDATORY_FLAGS = --error-exitcode=1
CPPCHECK_FLAGS ?= --enable=all
CPPCHECK_INPUT_FILES ?= $(top_srcdir)/src/*.cc

check-cpp:
ifeq ($(CPPCHECK_ENABLED),yes)
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_MANDATORY_FLAGS) $(CPPCHECK_INPUT_FILES)
else
	@echo "Need to reconfigure with --enable-cppcheck"
endif
'
	AC_SUBST([CPPCHECK_RULES])
])
