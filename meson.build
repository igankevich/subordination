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
if cpp.get_id() == 'gcc' or cpp.has_argument('-rdynamic')
	add_global_link_arguments('-rdynamic', language: 'cpp')
endif

# configuration
config = configuration_data()
config.set('kernel_type', get_option('kernel_type'))
config.set('kernel_header', get_option('kernel_header'))

threads = dependency('threads')
unistdx = dependency('unistdx')
gtest = dependency('gtest', main: true)

srcdir = include_directories('src')

meson.add_postconf_script('scripts/copy-input-files')
run_target('gcov', command: 'scripts/gcov-target')
run_target('iwyu', command: 'scripts/iwyu-target')
run_target('springy', command: 'scripts/springy-target')

subdir('src')

# Doxygen {{{
doxygen = find_program('doxygen', required: false)
if doxygen.found()
	run_target(
		'doc',
		command: [
			doxygen,
			join_paths(meson.source_root(), 'Doxyfile'),
		]
	)
endif
# }}}
cppcheck = find_program('cppcheck', required: false)
if cppcheck.found()
	run_target(
		'cppcheck',
		command: [
			cppcheck,
			'--enable=all',
			'--quiet',
			'--force',
			'--language=c++',
			'--std=c++11',
			'--template=gcc',
			'-I' + join_paths(meson.source_root(), 'src'),
			'-I' + join_paths(meson.build_root(), 'src'),
			join_paths(meson.source_root(), 'src'),
		]
	)
endif


