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

subdir('core')
subdir('daemon')
subdir('test')

if with_site
    subdir('doxygen')
    subdir('site')
endif

install_headers('api.hh', subdir: meson.project_name())
