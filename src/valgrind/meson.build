valgrind_conf = configuration_data()
valgrind_dep = dependency('valgrind', required: false)
if valgrind_dep.found()
    valgrind_dep = [valgrind_dep]
    valgrind_conf.set('SBN_TEST_HAVE_VALGRIND_H', true)
else
    valgrind_dep = []
endif
configure_file(
    input: 'config.hh.in',
    output: 'config.hh',
    configuration: valgrind_conf
)
