sbn_src = files([
    'application.cc',
    'basic_pipeline.cc',
    'basic_socket_pipeline.cc',
    'child_process_pipeline.cc',
    'connection.cc',
    'error.cc',
    'error_handler.cc',
    'factory.cc',
    'factory_properties.cc',
    'foreign_kernel.cc',
    'kernel.cc',
    'kernel_base.cc',
    'kernel_buffer.cc',
    'kernel_instance_registry.cc',
    'kernel_type.cc',
    'kernel_type_registry.cc',
    'parallel_pipeline.cc',
    'pipeline_base.cc',
    'process_handler.cc',
    'properties.cc',
    'resources.cc',
    'thread_pool.cc',
    'transaction_log.cc',
    'weights.cc',
])

install_headers(
    'application.hh',
    'basic_pipeline.hh',
    'basic_socket_pipeline.hh',
    'child_process_pipeline.hh',
    'connection.hh',
    'connection_table.hh',
    'error_handler.hh',
    'error.hh',
    'factory.hh',
    'foreign_kernel.hh',
    'kernel_base.hh',
    'kernel_buffer.hh',
    'kernel_header_flag.hh',
    'kernel.hh',
    'kernel_instance_registry.hh',
    'kernel_type.hh',
    'kernel_type_registry.hh',
    'parallel_pipeline.hh',
    'pipeline_base.hh',
    'process_handler.hh',
    'properties.hh',
    'resources.hh',
    'transaction_log.hh',
    'types.hh',
    'weights.hh',
    subdir: join_paths(meson.project_name(), 'core')
)

foreach config : binary_configs
    tmp_lib = library(
        config.get('prefix') + 'sbn',
        sources: sbn_src,
        cpp_args: config.get('cpp_args'),
        dependencies: [threads,unistdx],
        version: meson.project_version(),
        include_directories: src,
        install: config.get('prefix') == '',
        implicit_include_directories: false,
    )
    tmp_dep = declare_dependency(
        link_with: tmp_lib,
        include_directories: src,
        dependencies: [unistdx,threads],
    )
    set_variable(config.get('prefix') + 'sbn_lib', tmp_lib)
    set_variable(config.get('prefix') + 'sbn', tmp_dep)
endforeach

pkgconfig.generate(
    sbn_lib,
    requires: [unistdx],
    version: meson.project_version(),
    name: 'sbn',
    filebase: 'sbn',
    description: project_description,
)

clang_tidy_files += sbn_src

foreach name : [
    'kernel_buffer',
    'parallel_pipeline',
    'properties',
    'resources',
    'timer_pipeline',
    'weights',
]
    test_name = '-'.join(name.split('_'))
    exe_name = test_name + '-test'
    exe = executable(
        exe_name,
        sources: name + '_test.cc',
        include_directories: src,
        dependencies: [sbn, gtest] + valgrind_dep,
        implicit_include_directories: false,
    )
    test('core/' + test_name, exe)
endforeach
