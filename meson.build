project(
	'factory',
	'cpp',
	version: '0.2',
	meson_version: '>=0.31',
	default_options: ['cpp_std=c++14']
)

if get_option('buildtype') == 'release'
	add_global_arguments('-DNDEBUG', language: 'cpp')
endif

if get_option('springy')
	add_global_arguments('-DSPRINGY', language: 'cpp')
endif

cpp = meson.get_compiler('cpp')
libpthread = cpp.find_library('pthread')
srcdir = include_directories('src')

meson.add_postconf_script(meson.source_root() + '/scripts/copy-input-files')
run_target('cppcheck', meson.source_root() + '/scripts/cppcheck-target')
run_target('gcov', meson.source_root() + '/scripts/gcov-target')
run_target('iwyu', meson.source_root() + '/scripts/iwyu-target')
run_target('springy', meson.source_root() + '/scripts/springy-target')

subdir('src')
