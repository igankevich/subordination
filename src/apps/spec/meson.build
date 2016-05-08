libz = dependency('zlib')

flags3 = ['-static-libgcc', '-static-libstdc++', '-static', '-lz', '-lpthread' , '-lm']
if host_machine.system() == 'darwin'	
	flags3 = ['-lpthread', '-lm', '-lz']
endif

executable(
	'spec',
	sources: 'spec.cc',
	include_directories: [srcdir],
#	dependencies: [libpthread, librt, libgsl, liblapack, libblas],
#	link_args: ['-rdynamic', '-fopenmp', '-lgfortran'],
#	cpp_args: '-DFACTORY_PRIORITY_SCHEDULING',
	link_args: flags3
)
