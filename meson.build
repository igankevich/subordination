project('factory', 'cpp', version: '0.2', default_options: ['cpp_std=c++14'])

libpthread = find_library('pthread')
librt = find_library('rt')
threads = [libpthread, librt]
srcdir = include_directories('src')

cppcheck = find_program('cppcheck')
if cppcheck.found()
	run_target('cppcheck', meson.source_root() + '/scripts/cppcheck-target')
endif
run_target('gcov', meson.source_root() + '/scripts/gcov-target')

subdir('src')
