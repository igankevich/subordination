project('factory', 'cpp', version: '0.2', default_options: ['cpp_std=c++14'])

cpp = meson.get_compiler('cpp')

libpthread = cpp.find_library('pthread')
threads = [libpthread]

if host_machine.system() == 'darwin'
	static_link_flags = []
else
	static_link_flags = ['-static-libstdc++', '-static-libgcc']
endif

srcdir = include_directories('src')

cppcheck = find_program('cppcheck')
if cppcheck.found()
	run_target('cppcheck', meson.source_root() + '/scripts/cppcheck-target')
endif
run_target('gcov', meson.source_root() + '/scripts/gcov-target')
run_target('iwyu', meson.source_root() + '/scripts/iwyu-target')

subdir('src')
