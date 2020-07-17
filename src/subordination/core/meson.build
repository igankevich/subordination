sbn_src = files([
    'application.cc',
    'basic_factory.cc',
    'basic_pipeline.cc',
    'basic_socket_pipeline.cc',
    'child_process_pipeline.cc',
    'connection.cc',
    'error.cc',
    'error_handler.cc',
    'foreign_kernel.cc',
    'kernel_base.cc',
    'kernel_buffer.cc',
    'kernel.cc',
    'kernel_instance_registry.cc',
    'kernel_type.cc',
    'kernel_type_registry.cc',
    'parallel_pipeline.cc',
    'pipeline_base.cc',
    'process_handler.cc',
    'transaction_log.cc',
])

install_headers(
    'application.hh',
    'basic_factory.hh',
    'basic_pipeline.hh',
    'basic_socket_pipeline.hh',
    'child_process_pipeline.hh',
    'connection.hh',
    'connection_table.hh',
    'error.hh',
    'error_handler.hh',
    'foreign_kernel.hh',
    'kernel.hh',
    'kernel_base.hh',
    'kernel_buffer.hh',
    'kernel_header_flag.hh',
    'kernel_instance_registry.hh',
    'kernel_type.hh',
    'kernel_type_registry.hh',
    'parallel_pipeline.hh',
    'pipeline_base.hh',
    'process_handler.hh',
    'transaction_log.hh',
    'types.hh',
    subdir: join_paths(meson.project_name(), 'core')
)

sbn_lib = library(
    'sbn',
    sources: sbn_src,
    dependencies: [threads,unistdx] + libunwind + libdw,
    version: meson.project_version(),
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

sbn = declare_dependency(
    link_with: sbn_lib,
    include_directories: src,
    dependencies: [unistdx,threads] + libunwind + libdw
)

pkgconfig.generate(
    sbn_lib,
    requires: [unistdx] + libunwind + libdw,
    version: meson.project_version(),
    name: 'sbn',
    filebase: 'sbn',
    description: project_description,
)

foreach name : ['kernel_buffer', 'timer_pipeline']
    test_name = '-'.join(name.split('_'))
    exe_name = test_name + '-test'
    exe = executable(
        exe_name,
        sources: name + '_test.cc',
        include_directories: src,
        dependencies: [sbn, gtest],
        implicit_include_directories: false,
    )
    test('core/' + test_name, exe)
endforeach
