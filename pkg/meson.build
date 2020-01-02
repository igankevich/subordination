# RPM archive {{{
archive_name = meson.project_name() + '-' + meson.project_version()
full_archive_name = archive_name + '.tar.xz'
run_target(
    'rpm-archive',
    command: [
        'git',
        '-C',
        meson.source_root(),
        'archive',
        '-o',
        join_paths(meson.build_root(), full_archive_name),
        '--prefix',
        archive_name + '/',
        'HEAD'
    ]
)
# }}}
# RPM spec {{{
rpm_spec = configuration_data()
rpm_spec.set('name', meson.project_name())
rpm_spec.set('version', meson.project_version())
rpm_spec.set('release', '1')
rpm_spec.set('archive_name', full_archive_name)
rpm_spec.set('arch', build_machine.cpu_family())
rpm_spec.set('license', 'GPLv2')
configure_file(
    input: meson.project_name() + '.spec.in',
    output: meson.project_name() + '.spec',
    configuration: rpm_spec
)
# }}}
# RPM build {{{
rpm_setup_tree = find_program('rpmdev-setuptree', required: false)
if not rpm_setup_tree.found()
    rpm_setup_tree = find_program('rpmsetuptree', required: false)
endif
if rpm_setup_tree.found()
    run_target('rpm-setup-tree', command: rpm_setup_tree)
endif
rpm_build = find_program('rpmbuild', required: false)
if rpm_build.found()
    run_target(
        'rpm-build',
        command: [
            rpm_build,
            '-ba',
            join_paths(meson.current_build_dir(), meson.project_name() + '.spec'),
            '--define',
            '_sourcedir ' + meson.build_root()
        ]
    )
endif
# }}}
# RPM target {{{
ninja = find_program('ninja')
if ninja.found()
    run_target(
        'rpm',
        command: [
            ninja,
            '-C',
            meson.build_root(),
            'rpm-setup-tree',
            'rpm-archive',
            'rpm-build'
        ]
    )
endif
# }}}
# systemd service {{{
systemd_config = configuration_data()
systemd_config.set('prefix', get_option('prefix'))
systemd_config.set('sbindir', get_option('sbindir'))
systemd_config.set('subordination_exe', subordination_exe.full_path().split('/')[-1])
systemd_config.set('subordination_args', 'fanout=10000')
systemd_config.set('project_name', meson.project_name())
configure_file(
    input: 'subordination.service.in',
    output: meson.project_name() + '.service',
    configuration: systemd_config,
    install_dir: get_option('unitdir')
)
configure_file(
    input: 'subordination.in',
    output: meson.project_name(),
    configuration: systemd_config,
    install_dir: join_paths(get_option('sysconfdir'), 'sysconfig')
)
# }}}
