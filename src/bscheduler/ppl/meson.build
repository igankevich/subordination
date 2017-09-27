bscheduler_core_src += files([
	'application.cc',
	'application_kernel.cc',
	'basic_pipeline.cc',
	'io_pipeline.cc',
	'multi_pipeline.cc',
	'parallel_pipeline.cc',
	'thread_context.cc',
	'timer_pipeline.cc',
	'timer_pipeline.cc',
])

bscheduler_src += files([
	'basic_factory.cc',
	'external_process_handler.cc',
	'process_handler.cc',
	'process_pipeline.cc',
	'socket_pipeline.cc',
	'unix_domain_socket_pipeline.cc',
])

install_headers(
	'application.hh',
	'application_kernel.hh',
	'basic_factory.hh',
	'basic_pipeline.hh',
	'basic_router.hh',
	'basic_socket_pipeline.hh',
	'child_process_pipeline.hh',
	'compare_time.hh',
	'external_process_handler.hh',
	'io_pipeline.hh',
	'kernel_proto_flag.hh',
	'kernel_protocol.hh',
	'local_server.hh',
	'multi_pipeline.hh',
	'parallel_pipeline.hh',
	'pipeline_base.hh',
	'process_handler.hh',
	'process_pipeline.hh',
	'remote_client.hh',
	'socket_pipeline.hh',
	'socket_pipeline_event.hh',
	'thread_context.hh',
	'timer_pipeline.hh',
	'unix_domain_socket_pipeline.hh',
	subdir: join_paths(meson.project_name(), 'ppl')
)
