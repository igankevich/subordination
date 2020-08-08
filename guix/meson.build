scm = configuration_data()
scm.set('name', meson.project_name())
scm.set('description', project_description)
scm.set('version', meson.project_version())
scm.set('license', ' '.join(meson.project_license()))
scm.set('source_root', meson.source_root())
configure_file(input: 'package.scm.in', output: 'package.scm', configuration: scm)

guix = find_program('guix', required: false)
if guix.found()
    run_target(
        'package',
        command: [guix, 'build', '-f', join_paths(meson.current_build_dir(), 'package.scm')]
    )
endif
