python = import('python').find_installation(required: false)
doxygen = find_program('doxygen', required: false)
if doxygen.found() and python.found()
    theme_dir = join_paths(meson.current_source_dir(), 'theme')
    std_tag_xml = 'std.tag.xml'
    std_tag_url = 'http://en.cppreference.com/w/'
    build_py = configuration_data()
    build_py.set('build_root', meson.build_root())
    build_py.set('std_tag_xml', std_tag_xml)
    build_py.set('doxygen', doxygen.path())
    build_py.set('doxyfile', join_paths(meson.current_build_dir(), 'Doxyfile'))
    configure_file(input: 'build.py.in', output: 'build.py', configuration: build_py)
    doxyfile = configuration_data()
    doxyfile.set('OUTPUT_DIRECTORY', meson.build_root())
    excluded_files = [
        'meson.build',
        join_paths(meson.source_root(), 'src', meson.project_name(), 'test'),
        join_paths(meson.source_root(), 'doc', 'theme'),
    ]
    doxyfile.set('EXCLUDE', ' '.join(excluded_files))
    doxyfile.set('EXCLUDE_SYMBOLS', ' '.join(['bits']))
    input_files = [
        join_paths(meson.source_root(), 'src'),
        meson.current_source_dir(),
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
    doxyfile.set('LAYOUT_FILE', join_paths(theme_dir, 'layout.xml'))
    doxyfile.set('HTML_EXTRA_STYLESHEET', join_paths(theme_dir, 'doxygen-extra.css'))
    doxyfile.set('HTML_EXTRA_FILES', join_paths(theme_dir, 'extra.js'))
    doxyfile.set('HTML_HEADER', join_paths(theme_dir, 'header.html'))
    doxyfile.set('HTML_FOOTER', join_paths(theme_dir, 'footer.html'))
    configure_file(input: 'Doxyfile.in', output: 'Doxyfile', configuration: doxyfile)
    run_target(
        'doc',
        command: [python, join_paths(meson.current_build_dir(), 'build.py')],
    )
endif
