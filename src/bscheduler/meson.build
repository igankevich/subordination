configure_file(
	input: 'config.hh.in',
	output: 'config.hh',
	configuration: config
)

bscheduler_src = files()
bscheduler_core_src = files()

subdir('base')
subdir('kernel')
subdir('ppl')

bscheduler_core_lib = shared_library(
	'bscheduler-core',
	sources: bscheduler_core_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	include_directories: srcdir,
)

bscheduler_core = declare_dependency(
	link_with: bscheduler_core_lib,
	include_directories: srcdir
)

bscheduler_daemon_lib = shared_library(
	'bscheduler-daemon',
	sources: bscheduler_src,
	dependencies: [threads,unistdx,bscheduler_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DBSCHEDULER_DAEMON'] + profiling_args,
)

bscheduler_submit_lib = shared_library(
	'bscheduler-submit',
	sources: bscheduler_src,
	dependencies: [threads,unistdx,bscheduler_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DBSCHEDULER_SUBMIT'],
)

bscheduler_app_lib = shared_library(
	'bscheduler-app',
	sources: bscheduler_src,
	dependencies: [threads,unistdx,bscheduler_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DBSCHEDULER_APPLICATION'],
)

bscheduler_daemon = declare_dependency(
	link_with: [bscheduler_daemon_lib,bscheduler_core_lib],
	include_directories: srcdir
)

bscheduler_submit = declare_dependency(
	link_with: [bscheduler_submit_lib,bscheduler_core_lib],
	include_directories: srcdir
)

bscheduler_app = declare_dependency(
	link_with: [bscheduler_app_lib,bscheduler_core_lib],
	include_directories: srcdir
)

subdir('daemon')
