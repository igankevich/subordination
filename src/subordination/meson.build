subdir('test')
subdir('core')
subdir('daemon')
subdir('guile')
subdir('python')

if with_site
    subdir('doxygen')
    subdir('site')
endif

install_headers('api.hh', subdir: meson.project_name())
