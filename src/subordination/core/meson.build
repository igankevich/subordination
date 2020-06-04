subordination_core_src = files([
    'exit_code.cc',
    'foreign_kernel.cc',
    'kernel.cc',
    'kernel_error.cc',
    'kernel_header.cc',
    'kernel_instance_registry.cc',
    'kernel_type.cc',
    'kernel_type_error.cc',
    'kernel_type_registry.cc',
    'mobile_kernel.cc',
    'application.cc',
    'application_kernel.cc',
    'basic_factory.cc',
    'basic_pipeline.cc',
    'basic_socket_pipeline.cc',
    'child_process_pipeline.cc',
    'kernel_protocol.cc',
    'parallel_pipeline.cc',
    'pipeline_base.cc',
    'process_handler.cc',
    'process_pipeline.cc',
    'socket_pipeline.cc',
    'error.cc',
    'error_handler.cc',
])

install_headers(
    'act.hh',
    'exit_code.hh',
    'foreign_kernel.hh',
    'kernel.hh',
    'kernel_base.hh',
    'kernel_error.hh',
    'kernel_flag.hh',
    'kernel_header.hh',
    'kernel_instance_registry.hh',
    'kernel_type.hh',
    'kernel_type_error.hh',
    'kernel_type_registry.hh',
    'kernelbuf.hh',
    'kstream.hh',
    'mobile_kernel.hh',
    'application.hh',
    'application_kernel.hh',
    'basic_factory.hh',
    'basic_handler.hh',
    'basic_pipeline.hh',
    'basic_socket_pipeline.hh',
    'child_process_pipeline.hh',
    'kernel_header_flag.hh',
    'kernel_proto_flag.hh',
    'kernel_protocol.hh',
    'local_server.hh',
    'parallel_pipeline.hh',
    'pipeline_base.hh',
    'process_handler.hh',
    'process_pipeline.hh',
    'socket_pipeline.hh',
    'socket_pipeline_event.hh',
    'error_handler.hh',
    'error.hh',
    'static_lock.hh',
    subdir: join_paths(meson.project_name(), 'kernel')
)

subordination_core_lib = library(
    'subordination-core',
    sources: subordination_core_src,
    dependencies: [threads,unistdx],
    version: meson.project_version(),
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

subordination_core = declare_dependency(
    link_with: subordination_core_lib,
    include_directories: src
)

pkgconfig.generate(
    subordination_core_lib,
    requires: [unistdx],
    version: meson.project_version(),
    name: 'subordination-core',
    filebase: 'subordination-core',
    description: project_description,
)

test(
    'kstream-test',
    executable(
        'kstream-test',
        sources: 'kstream_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_core],
        implicit_include_directories: false,
    )
)
