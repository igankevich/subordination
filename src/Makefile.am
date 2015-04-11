AM_CPPFLAGS = -Wall -Wextra -Wpedantic -Wunused -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value -Wunused-variable -Wvariadic-macros -Wvla -Wvolatile-register-var -Wwrite-strings -Wtype-limits -Wundef -Wuninitialized -Wunknown-pragmas -Wno-pragmas -Wunreachable-code -Wswitch -Wswitch-default -Wswitch-enum -Wsync-nand -Wredundant-decls -Wreturn-type -Wsequence-point -Wshadow -Wsign-compare -Wsign-conversion -Wstrict-overflow=5 -Wpacked -Wpacked-bitfield-compat -Wparentheses -Wpointer-arith -Wno-multichar -Wnonnull -Wno-overflow -Woverlength-strings -Wmissing-include-dirs -Wmissing-noreturn -Wmissing-field-initializers -Wmissing-format-attribute -Winvalid-pch -Wlogical-op -Wmain -Wmissing-braces -Wno-int-to-pointer-cast -Wno-invalid-offsetof -Wignored-qualifiers -Winit-self -Wno-endif-labels -Wfatal-errors -Wfloat-equal -Wno-div-by-zero -Wempty-body -Wenum-compare -Wno-deprecated -Wno-deprecated-declarations -Wdisabled-optimization -Wcomment -Wconversion -Wcoverage-mismatch -Wcast-align -Wcast-qual -Wchar-subscripts -Wclobbered -Wc++11-compat -Waddress -Warray-bounds -Wno-attributes -Wno-builtin-macro-redefined -Wno-pmf-conversions -Wsign-promo -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder -Weffc++ -Wstrict-null-sentinel -Wno-non-template-friend -Wstrict-aliasing=1 -Wold-style-cast -Wunsafe-loop-optimizations 

#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed


TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_spinlock \
	test_endpoint

check_PROGRAMS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_discovery \
	test_spinlock \
	test_endpoint

test_buffer_SOURCES     = test_buffer.cc
test_marshaling_SOURCES = test_marshaling.cc
test_socket_SOURCES     = test_socket.cc
test_offline_SOURCES    = test_offline.cc
test_discovery_SOURCES  = test_discovery.cc
test_spinlock_SOURCES   = test_spinlock.cc
test_endpoint_SOURCES   = test_endpoint.cc
