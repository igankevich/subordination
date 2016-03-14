project('factory', 'cpp', version: '0.2', default_options: ['cpp_std=c++14'])

libpthread = find_library('pthread')
librt = find_library('rt')
libeigen3 = dependency('eigen3')
threads = [libpthread]
#threads = [libpthread, librt]

static_link_flags = ['-static-libstdc++', '-static-libgcc']

srcdir = include_directories('src')

cppcheck = find_program('cppcheck')
if cppcheck.found()
	run_target('cppcheck', meson.source_root() + '/scripts/cppcheck-target')
endif
run_target('gcov', meson.source_root() + '/scripts/gcov-target')
run_target('iwyu', meson.source_root() + '/scripts/iwyu-target')

subdir('src')
