subdir('core')
subdir('daemon')
subdir('test')

if with_site
    subdir('doxygen')
    subdir('site')
endif

install_headers('api.hh', subdir: meson.project_name())
