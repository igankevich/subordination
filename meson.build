project(
	'bscheduler',
	'cpp',
	version: '0.2',
	meson_version: '>=0.42.0',
	default_options: ['cpp_std=c++14']
)

if get_option('buildtype') == 'release'
	add_global_arguments('-DNDEBUG', language: 'cpp')
endif

profiling_args = []
if get_option('profile_node_discovery')
	profiling_args += ['-DBSCHEDULER_PROFILE_NODE_DISCOVERY']
endif

cpp = meson.get_compiler('cpp')
if cpp.get_id() == 'gcc' or cpp.has_argument('-rdynamic')
	add_global_link_arguments('-rdynamic', language: 'cpp')
endif

# configuration
log_dir = join_paths(get_option('localstatedir'), 'log', 'bscheduler')
config = configuration_data()
config.set('kernel_type', get_option('kernel_type'))
config.set('kernel_header', get_option('kernel_header'))
config.set('logdir', log_dir)

threads = dependency('threads')
unistdx = dependency('unistdx')
gtest = dependency('gtest', main: true)

srcdir = include_directories('src')

subdir('src')

# doxygen {{{
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
# cppcheck {{{
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
# }}}
# systemd service {{{
systemd_config = configuration_data()
systemd_config.set('prefix', get_option('prefix'))
systemd_config.set('sbindir', get_option('sbindir'))
systemd_config.set('bscheduler_exe', bscheduler_exe.full_path().split('/')[-1])
systemd_config.set('bscheduler_args', 'fanout=10000')
systemd_config.set('project_name', meson.project_name())
configure_file(
	input: 'bscheduler.service.in',
	output: meson.project_name() + '.service',
	configuration: systemd_config,
	install_dir: get_option('unitdir')
)
configure_file(
	input: 'bscheduler.in',
	output: meson.project_name(),
	configuration: systemd_config,
	install_dir: join_paths(get_option('sysconfdir'), 'sysconfig')
)
# }}}
# pkgconfig {{{
pkgconfig = import('pkgconfig')
pkgconfig.generate(
	libraries: bscheduler_core_lib,
	version: meson.project_version(),
	name: 'bscheduler-core',
	filebase: 'bscheduler-core',
	description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
	libraries: [bscheduler_app_lib, bscheduler_core_lib],
	extra_cflags: '-DBSCHEDULER_APPLICATION',
	version: meson.project_version(),
	name: 'bscheduler-app',
	filebase: 'bscheduler-app',
	description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
	libraries: [bscheduler_daemon_lib, bscheduler_core_lib],
	extra_cflags: '-DBSCHEDULER_DAEMON',
	version: meson.project_version(),
	name: 'bscheduler-daemon',
	filebase: 'bscheduler-daemon',
	description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
	libraries: [bscheduler_submit_lib, bscheduler_core_lib],
	extra_cflags: '-DBSCHEDULER_SUBMIT',
	version: meson.project_version(),
	name: 'bscheduler-submit',
	filebase: 'bscheduler-submit',
	description: 'Distributed programming C++ framework',
)
# }}}
