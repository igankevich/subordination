dnl @synopsis FACTORY_CXX_ENABLE_ALL_WARNINGS
dnl @summary enable C++ compiler warnings
AC_DEFUN([FACTORY_CXX_ENABLE_ALL_WARNINGS],
[dnl
	FACTORY_CXX_FLAG([-Wall])
	FACTORY_CXX_FLAG([-Wextra])
	FACTORY_CXX_FLAG([-Wpedantic])
	FACTORY_CXX_FLAG([-Weverything])
	FACTORY_CXX_FLAG([-Wunused])
	FACTORY_CXX_FLAG([-Wunused-function])
	FACTORY_CXX_FLAG([-Wunused-label])
	FACTORY_CXX_FLAG([-Wunused-parameter])
	FACTORY_CXX_FLAG([-Wunused-value])
	FACTORY_CXX_FLAG([-Wunused-variable])
	FACTORY_CXX_FLAG([-Wvariadic-macros])
	FACTORY_CXX_FLAG([-Wvla])
	FACTORY_CXX_FLAG([-Wvolatile-register-var])
	FACTORY_CXX_FLAG([-Wwrite-strings])
	FACTORY_CXX_FLAG([-Wtype-limits])
	FACTORY_CXX_FLAG([-Wundef])
	FACTORY_CXX_FLAG([-Wuninitialized])
	FACTORY_CXX_FLAG([-Wunknown-pragmas])
	FACTORY_CXX_FLAG([-Wno-pragmas])
	FACTORY_CXX_FLAG([-Wunreachable-code])
	FACTORY_CXX_FLAG([-Wswitch])
	FACTORY_CXX_FLAG([-Wswitch-default])
	FACTORY_CXX_FLAG([-Wswitch-enum])
	FACTORY_CXX_FLAG([-Wsync-nand])
	FACTORY_CXX_FLAG([-Wredundant-decls])
	FACTORY_CXX_FLAG([-Wreturn-type])
	FACTORY_CXX_FLAG([-Wsequence-point])
	FACTORY_CXX_FLAG([-Wshadow])
	FACTORY_CXX_FLAG([-Wsign-compare])
	FACTORY_CXX_FLAG([-Wsign-conversion])
	FACTORY_CXX_FLAG([-Wstrict-overflow=5])
	FACTORY_CXX_FLAG([-Wpacked])
	FACTORY_CXX_FLAG([-Wpacked-bitfield-compat])
	FACTORY_CXX_FLAG([-Wparentheses])
	FACTORY_CXX_FLAG([-Wpointer-arith])
	FACTORY_CXX_FLAG([-Wno-multichar])
	FACTORY_CXX_FLAG([-Wnonnull])
	FACTORY_CXX_FLAG([-Wno-overflow])
	FACTORY_CXX_FLAG([-Woverlength-strings])
	FACTORY_CXX_FLAG([-Wmissing-include-dirs])
	FACTORY_CXX_FLAG([-Wmissing-noreturn])
	FACTORY_CXX_FLAG([-Wmissing-field-initializers])
	FACTORY_CXX_FLAG([-Wmissing-format-attribute])
	FACTORY_CXX_FLAG([-Winvalid-pch])
	FACTORY_CXX_FLAG([-Wlogical-op])
	FACTORY_CXX_FLAG([-Wmain])
	FACTORY_CXX_FLAG([-Wmissing-braces])
	FACTORY_CXX_FLAG([-Wno-int-to-pointer-cast])
	FACTORY_CXX_FLAG([-Wno-invalid-offsetof])
	FACTORY_CXX_FLAG([-Wignored-qualifiers])
	FACTORY_CXX_FLAG([-Winit-self])
	FACTORY_CXX_FLAG([-Wno-endif-labels])
	FACTORY_CXX_FLAG([-Wfatal-errors])
	FACTORY_CXX_FLAG([-Wfloat-equal])
	FACTORY_CXX_FLAG([-Wno-div-by-zero])
	FACTORY_CXX_FLAG([-Wempty-body])
	FACTORY_CXX_FLAG([-Wenum-compare])
	FACTORY_CXX_FLAG([-Wno-deprecated])
	FACTORY_CXX_FLAG([-Wno-deprecated-declarations])
	FACTORY_CXX_FLAG([-Wdisabled-optimization])
	FACTORY_CXX_FLAG([-Wcomment])
	FACTORY_CXX_FLAG([-Wconversion])
	FACTORY_CXX_FLAG([-Wcoverage-mismatch])
	FACTORY_CXX_FLAG([-Wcast-align])
	FACTORY_CXX_FLAG([-Wcast-qual])
	FACTORY_CXX_FLAG([-Wchar-subscripts])
	FACTORY_CXX_FLAG([-Wclobbered])
	FACTORY_CXX_FLAG([-Wc++11-compat])
	FACTORY_CXX_FLAG([-Waddress])
	FACTORY_CXX_FLAG([-Warray-bounds])
	FACTORY_CXX_FLAG([-Wno-attributes])
	FACTORY_CXX_FLAG([-Wno-builtin-macro-redefined])
	FACTORY_CXX_FLAG([-Wno-pmf-conversions])
	FACTORY_CXX_FLAG([-Wsign-promo])
	FACTORY_CXX_FLAG([-Woverloaded-virtual])
	FACTORY_CXX_FLAG([-Wctor-dtor-privacy])
	FACTORY_CXX_FLAG([-Wnon-virtual-dtor])
	FACTORY_CXX_FLAG([-Wreorder])
	FACTORY_CXX_FLAG([-Weffc++])
	FACTORY_CXX_FLAG([-Wstrict-null-sentinel])
	FACTORY_CXX_FLAG([-Wno-non-template-friend])
	FACTORY_CXX_FLAG([-Wstrict-aliasing=1])
	FACTORY_CXX_FLAG([-Wold-style-cast])
])
