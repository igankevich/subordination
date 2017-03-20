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

if get_option('handle_multiple_node_failures')
	add_global_arguments('-DHANDLE_MULTIPLE_NODE_FAILURES', language: 'cpp')
endif

if get_option('profile_multiple_node_failures')
	add_global_arguments('-DPROFILE_MULTIPLE_NODE_FAILURES', language: 'cpp')
endif

if get_option('dry_run')
	add_global_arguments('-DFACTORY_TEST_DRY_RUN', language: 'cpp')
endif

unistdx = dependency('unistdx')
stdx = dependency('stdx')
cpp = meson.get_compiler('cpp')
libpthread = cpp.find_library('pthread')
srcdir = include_directories('src')

meson.add_postconf_script('scripts/copy-input-files')
run_target('cppcheck', command: 'scripts/cppcheck-target')
run_target('gcov', command: 'scripts/gcov-target')
run_target('iwyu', command: 'scripts/iwyu-target')
run_target('springy', command: 'scripts/springy-target')
run_target('doc', command: 'scripts/doc-target')

subdir('src')
