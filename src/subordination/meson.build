configure_file(
    input: 'config.hh.in',
    output: 'config.hh',
    configuration: config,
    install_dir: join_paths(
        get_option('prefix'),
        get_option('includedir'),
        meson.project_name()
    )
)

subordination_src = files()
subordination_core_src = files()

subdir('base')
subdir('kernel')
subdir('ppl')

install_headers(
    'api.hh',
    subdir: meson.project_name()
)

subordination_core_lib = shared_library(
    'subordination-core',
    sources: subordination_core_src,
    dependencies: [threads,unistdx],
    version: meson.project_version(),
    include_directories: src,
    install: true,
)

subordination_core = declare_dependency(
    link_with: subordination_core_lib,
    include_directories: src
)

subordination_daemon_lib = shared_library(
    'subordination-daemon',
    sources: subordination_src,
    dependencies: [threads,unistdx,subordination_core],
    version: meson.project_version(),
    install: true,
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_DAEMON'] + profiling_args,
)

subordination_submit_lib = shared_library(
    'subordination-submit',
    sources: subordination_src,
    dependencies: [threads,unistdx,subordination_core],
    version: meson.project_version(),
    install: true,
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_SUBMIT'],
)

subordination_app_lib = shared_library(
    'subordination-app',
    sources: subordination_src,
    dependencies: [threads,unistdx,subordination_core],
    version: meson.project_version(),
    install: true,
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_APPLICATION'],
)

subordination_daemon = declare_dependency(
    link_with: [subordination_daemon_lib,subordination_core_lib],
    include_directories: src
)

subordination_submit = declare_dependency(
    link_with: [subordination_submit_lib,subordination_core_lib],
    include_directories: src
)

subordination_app = declare_dependency(
    link_with: [subordination_app_lib,subordination_core_lib],
    include_directories: src
)

subdir('daemon')
if with_site
    subdir('doxygen')
    subdir('site')
endif
