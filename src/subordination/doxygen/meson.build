python = import('python').find_installation(required: false)
doxygen = find_program('doxygen')
doxyfile_path = join_paths(meson.current_build_dir(), 'Doxyfile')

html_dir = join_paths(meson.build_root(), 'html')
std_tag_xml = join_paths(meson.build_root(), 'std.tag.xml')
std_tag_url = 'http://en.cppreference.com/w/'
build_py = configuration_data()
build_py.set('build_root', meson.build_root())
build_py.set('current_source_dir', meson.current_source_dir())
build_py.set('std_tag_xml', std_tag_xml)
build_py.set('doxygen', doxygen.path())
build_py.set('doxyfile', doxyfile_path)
configure_file(input: 'build.py.in', output: 'build.py', configuration: build_py)
doxyfile = configuration_data()
doxyfile.set('PROJECT_BRIEF', project_description)
doxyfile.set('PROJECT_NUMBER', meson.project_version())
doxyfile.set('OUTPUT_DIRECTORY', meson.build_root())
excluded_files = [
    meson.build_root(),
    meson.current_source_dir(),
]
doxyfile.set('EXCLUDE', ' '.join(excluded_files))
doxyfile.set('EXCLUDE_PATTERNS', ' '.join(['*.css', '*.scm', '*_test.cc', 'meson.build',
                                           '*~', '.*']))
doxyfile.set('EXCLUDE_SYMBOLS', ' '.join(['bits']))
input_files = [
    join_paths(meson.source_root(), 'src', meson.project_name()),
]
doxyfile.set('INPUT', ' '.join(input_files))
perl = find_program('perl', required: false)
if perl.found()
    doxyfile.set('PERL_PATH', perl.path())
else
    doxyfile.set('PERL_PATH', '')
endif
doxyfile.set('TAGFILES', std_tag_xml + '=' + std_tag_url)
xml_output_dir = meson.current_build_dir()
doxyfile.set('XML_OUTPUT', xml_output_dir)
configure_file(input: 'Doxyfile.in', output: 'Doxyfile', configuration: doxyfile)

doxygen_xml = [custom_target(
    'doxygen',
    command: [python, join_paths(meson.current_build_dir(), 'build.py')],
    output: 'index.xml',
    build_always_stale: true
)]
