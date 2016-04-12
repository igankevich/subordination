libz = dependency('zlib')

executable(
	'spec',
	sources: 'spec.cc',
	include_directories: [srcdir],
#	dependencies: [libpthread, librt, libgsl, liblapack, libblas],
#	link_args: ['-rdynamic', '-fopenmp', '-lgfortran'],
#	cpp_args: '-DFACTORY_PRIORITY_SCHEDULING',
	link_args: ['-static-libgcc', '-static-libstdc++', '-static', '-lz', '-lpthread' , '-lm']
)
