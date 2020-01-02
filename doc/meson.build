python = import('python').find_installation(required: false)
doxygen = find_program('doxygen', required: false)
if doxygen.found() and python.found()
    html_dir = join_paths(meson.build_root(), 'html')
    std_tag_xml = 'std.tag.xml'
    std_tag_url = 'http://en.cppreference.com/w/'
    build_py = configuration_data()
    build_py.set('build_root', meson.build_root())
    build_py.set('std_tag_xml', std_tag_xml)
    build_py.set('doxygen', doxygen.path())
    build_py.set('doxyfile', join_paths(meson.current_build_dir(), 'Doxyfile'))
    configure_file(input: 'build.py.in', output: 'build.py', configuration: build_py)
    doxyfile = configuration_data()
    doxyfile.set('PROJECT_BRIEF', project_description)
    doxyfile.set('OUTPUT_DIRECTORY', meson.build_root())
    excluded_files = [
        'meson.build',
        join_paths(meson.source_root(), 'src', meson.project_name(), 'test'),
        join_paths(meson.source_root(), 'src', 'examples'),
    ]
    doxyfile.set('EXCLUDE', ' '.join(excluded_files))
    doxyfile.set('EXCLUDE_SYMBOLS', ' '.join(['bits']))
    input_files = [
        join_paths(meson.source_root(), 'src'),
        join_paths(meson.source_root(), 'doc'),
        join_paths(meson.source_root(), 'doc', 'pages'),
    ]
    doxyfile.set('INPUT', ' '.join(input_files))
    perl = find_program('perl', required: false)
    if perl.found()
        doxyfile.set('PERL_PATH', perl.path())
    else
        doxyfile.set('PERL_PATH', '')
    endif
    dot = find_program('dot', required: false)
    if dot.found()
        doxyfile.set('DOT_PATH', dot.path())
        doxyfile.set('HAVE_DOT', 'YES')
    else
        doxyfile.set('DOT_PATH', '')
        doxyfile.set('HAVE_DOT', 'NO')
    endif
    doxyfile.set('TAGFILES', std_tag_xml + '=' + std_tag_url)
    doxyfile.set('LAYOUT_FILE', 'layout.xml')
    doxyfile.set('HTML_EXTRA_STYLESHEET', '')
    doxyfile.set('HTML_EXTRA_FILES', '')
    doxyfile.set('HTML_HEADER', '')
    doxyfile.set('HTML_FOOTER', '')
    doxyfile.set('HTML_OUTPUT', html_dir)
    configure_file(input: 'Doxyfile.in', output: 'Doxyfile', configuration: doxyfile)
    run_target(
        'doc',
        command: [python, join_paths(meson.current_build_dir(), 'build.py')],
    )
    run_target('doc-server', command: [python, '-m', 'http.server', '--directory', html_dir])
endif
