AC_DEFUN([FACTORY_PROFILING],[
	# allow to override gcov location
	AC_ARG_WITH([gprof],
	  [AS_HELP_STRING([--with-gprof[=GPROF]], [use given GPROF for coverage (GPROF=gprof).])],
	  [_FACTORY_PERF_PROFILING_GPROF_PROG_WITH=$with_gprof],
	  [_FACTORY_PERF_PROFILING_GPROF_PROG_WITH=gprof])

	AC_MSG_CHECKING([whether to enable Gprof on the unit tests])
	AC_ARG_ENABLE([perf-testing],
	  AS_HELP_STRING([--enable-perf-testing],
	  [Whether to enable performance tests]),,
	  enable_perf_testing=no)

	AM_CONDITIONAL([PERF_TESTING_ENABLED], [test x$enable_perf_testing = xyes])
	AC_SUBST([PERF_TESTING_ENABLED], [$enable_perf_testing])
	AC_MSG_RESULT($enable_perf_testing)

	AS_IF([ test "$enable_perf_testing" = "yes" ], [
		# check for gprof
		AC_CHECK_TOOL([GPROF],
		  [$_FACTORY_PERF_PROFILING_GPROF_PROG_WITH],
		  [:])
		AS_IF([test "X$GPROF" = "X:"],
		  [AC_MSG_ERROR([gprof is needed to do performance testing])])
		AC_SUBST([GPROF])

		dnl Check if gcc is being used
		AS_IF([ test "$GCC" = "no" ], [
			AC_MSG_ERROR([not compiling with gcc, which is required for gprof performance testing])
		])

		dnl Build the performance testing flags
		PERF_TESTING_CFLAGS="-O0 -pg"
		PERF_TESTING_LDFLAGS=""

		AC_SUBST([PERF_TESTING_CFLAGS])
		AC_SUBST([PERF_TESTING_LDFLAGS])
	])

PERF_TESTING_RULES='
# Performance testing
#
# The generated report will be titled using the $(PACKAGE_NAME) and
# $(PACKAGE_VERSION). In order to add the current git hash to the title,
# use the git-version-gen script, available online.

# Optional variables
PERF_TESTING_OUTPUT_FILE ?= $(PACKAGE_NAME)-$(PACKAGE_VERSION)-gmon.info
PERF_TESTING_PREFIX = "gmon"

# Use recursive makes in order to ignore errors during check
check-performance:
ifeq ($(PERF_TESTING_ENABLED),yes)
	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)
	-rm -f $(PERF_TESTING_OUTPUT_FILE)
	-$(foreach test,$(TESTS), \
		rm -f $(test)-$(PERF_TESTING_PREFIX).*; \
		GMON_OUT_PREFIX=$(test)-$(PERF_TESTING_PREFIX) ./$(test) >$(test).log 2>&1; \
		$(GPROF) $(test) $(test)-$(PERF_TESTING_PREFIX).* >> $(PERF_TESTING_OUTPUT_FILE); \
		rm -f $(test)-$(PERF_TESTING_PREFIX).*; \
	)
	$(MAKE) $(AM_MAKEFLAGS) performance-capture
else
	@echo "Need to reconfigure with --enable-performance"
endif

# Capture profiling data
performance-capture: performance-capture-hook
ifeq ($(PERF_TESTING_ENABLED),yes)
	@echo "file://$(abs_builddir)/$(PERF_TESTING_OUTPUT_FILE)"
else
	@echo "Need to reconfigure with --enable-performance"
endif

# Hook rule executed before performance-capture, overridable by the user
performance-capture-hook:

ifeq ($(PERF_TESTING_ENABLED),yes)
clean: performance-clean
performance-clean:
	-rm $(PERF_TESTING_OUTPUT_FILE) $(PERF_TESTING_PREFIX).*
endif

GITIGNOREFILES ?=
GITIGNOREFILES += $(PERF_TESTING_OUTPUT_FILE) $(PERF_TESTING_OUTPUT_DIRECTORY)

DISTCHECK_CONFIGURE_FLAGS ?=
DISTCHECK_CONFIGURE_FLAGS += --disable-perf-testing

.PHONY: check-performance performance-capture performance-capture-hook performance-clean
'

	AC_SUBST([PERF_TESTING_RULES])
	m4_ifdef([_AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE([PERF_TESTING_RULES])])
])
