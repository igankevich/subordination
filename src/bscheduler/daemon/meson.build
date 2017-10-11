bscheduler_src = files([
	'bscheduler.cc',
	'hierarchy.cc',
	'hierarchy_kernel.cc',
	'hierarchy_node.cc',
	'master_discoverer.cc',
	'network_master.cc',
	'position_in_tree.cc',
	'probe.cc',
	'prober.cc',
	'resident_kernel.cc',
	'tree_hierarchy_iterator.cc',
])

bscheduler_exe = executable(
	'bscheduler',
	sources: bscheduler_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_daemon],
	cpp_args: ['-DBSCHEDULER_DAEMON'] + profiling_args,
	install: true,
	install_dir: get_option('sbindir')
)

bsub_src = files([
	'bsub.cc',
])

bsub_exe = executable(
	'bsub',
	sources: bsub_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_submit],
	cpp_args: ['-DBSCHEDULER_SUBMIT'],
	install: true
)

test_application = executable(
	'test-application',
	sources: 'test_application.cc',
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_app],
	cpp_args: ['-DBSCHEDULER_APPLICATION'],
)

test(
	'tree-hierarchy-iterator-test',
	executable(
		'tree-hierarchy-iterator-test',
		sources: [
			'position_in_tree.cc',
			'tree_hierarchy_iterator.cc',
			'tree_hierarchy_iterator_test.cc'
		],
		dependencies: [gtest,unistdx,threads,bscheduler_daemon],
		cpp_args: ['-DBSCHEDULER_DEBUG_TREE_HIERARCHY_ITERATOR']
	)
)


if get_option('buildtype').contains('debug')
	discovery_test = executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir],
		dependencies: [unistdx,threads,bscheduler_daemon,gtest],
		cpp_args: ['-DBSCHEDULER_DAEMON'],
	)
	foreach nodes : ['2', '4', '8']
		foreach fanout : ['1000', '2']
			suffix = '-' + nodes + '-' + fanout
			test_env = environment()
			test_env.set(
				'_CLUSTER',
				join_paths(meson.source_root(), 'src', 'test', 'cluster')
			)
			test_env.set('_LOGDIR', join_paths(meson.build_root(), 'logs' + suffix))
			test_env.set('_NODES', nodes)
			test_env.set('_FANOUT', fanout)
			test_env.set('_NAME', 'c' + suffix + '-')
			if nodes == '2' and fanout == '2'
				test_env.set(
					'_SUBMIT',
					bsub_exe.full_path() + ' ' +
					'/home/igankevich/workspace/arma/build/src/arma' + ' ' +
					'/home/igankevich/workspace/arma/input/standing_wave.arma'
#					bsub_exe.full_path() + ' ' + test_application.full_path()
				)
			endif
			# run discovery test over virtual network
			# using Linux namespaces
			test(
				'discovery-test' + suffix,
				discovery_test,
				args: [
					bscheduler_exe.full_path(),
					'fanout=' + fanout,
					'allow_root=1'
				],
				env: test_env,
				workdir: meson.build_root()
			)
		endforeach
	endforeach
endif
