project('factory', 'cpp', version: '0.2', default_options: ['cpp_std=c++14'])

libpthread = find_library('pthread')
librt = find_library('rt')
threads = [libpthread, librt]
srcdir = include_directories('src')

cppcheck = find_program('cppcheck')
if cppcheck.found()
	run_target('cppcheck', meson.source_root() + '/scripts/cppcheck')
endif
run_target('coverage', meson.source_root() + '/scripts/lcov-genhtml')

subdir('src')
