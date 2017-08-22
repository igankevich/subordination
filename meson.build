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
threads = dependency('threads')
srcdir = include_directories('src')

meson.add_postconf_script('scripts/copy-input-files')
run_target('cppcheck', command: 'scripts/cppcheck-target')
run_target('gcov', command: 'scripts/gcov-target')
run_target('iwyu', command: 'scripts/iwyu-target')
run_target('springy', command: 'scripts/springy-target')
run_target('doc', command: 'scripts/doc-target')

unistdx = dependency('unistdx')

subdir('src')
