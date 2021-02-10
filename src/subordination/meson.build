subdir('test')
subdir('bits')
subdir('core')
subdir('daemon')
if with_guile
    subdir('guile')
endif
if with_python
    subdir('python')
endif

if with_site
    subdir('doxygen')
    subdir('site')
endif

install_headers('api.hh', subdir: meson.project_name())
