empty_exe = executable('empty', sources: 'empty.cc')
test_config = configuration_data()
test_config.set('empty_exe_path', empty_exe.full_path())
configure_file(
    input: 'config.hh.in',
    output: 'config.hh',
    configuration: test_config
)

test_env = environment()
if get_option('b_sanitize') == 'address'
    test_env.set(
        'LSAN_OPTIONS',
        'suppressions=' + join_paths(meson.current_source_dir(), 'lsan.supp')
    )
endif
