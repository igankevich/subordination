configure_file(
    input: 'config.hh.in',
    output: 'config.hh',
    #configuration: configuration_data(),
    copy: true,
    install_dir: join_paths(
        get_option('prefix'),
        get_option('includedir'),
        meson.project_name()
    )
)

subordination_core_src = files()

subdir('base')
subdir('kernel')
subdir('ppl')

install_headers(
    'api.hh',
    subdir: meson.project_name()
)

subordination_core_lib = library(
    'subordination-core',
    sources: subordination_core_src,
    dependencies: [threads,unistdx],
    version: meson.project_version(),
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

subordination_core = declare_dependency(
    link_with: subordination_core_lib,
    include_directories: src
)

pkgconfig.generate(
    subordination_core_lib,
    requires: [unistdx],
    version: meson.project_version(),
    name: 'subordination-core',
    filebase: 'subordination-core',
    description: project_description,
)

subdir('daemon')
if with_site
    subdir('doxygen')
    subdir('site')
endif
