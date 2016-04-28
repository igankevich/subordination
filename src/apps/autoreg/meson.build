#libgsl = dependency('gsl')
liblapack = cpp.find_library('lapack')
libblas = cpp.find_library('blas')
#libgfortran = find_library('gfortran')

executable(
	'autoreg',
	sources: 'autoreg.cc',
	include_directories: [srcdir],
#	dependencies: [libpthread, librt, libgsl, liblapack, libblas],
#	link_args: ['-rdynamic', '-fopenmp', '-lgfortran'],
#	cpp_args: '-fopenmp'
	link_args: ['-static-libgcc', '-static-libstdc++', '-static', '-llapack', '-lblas', '-lgfortran', '-lquadmath', '-lpthread' , '-lm']
)
