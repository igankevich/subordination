libgsl = dependency('gsl')
liblapack = find_library('lapack')
libblas = find_library('blas')
#libgfortran = find_library('gfortran')

executable(
	'autoreg',
	sources: 'autoreg.cc',
	include_directories: [srcdir],
	dependencies: [libpthread, librt, libgsl, liblapack, libblas],
	link_args: ['-rdynamic', '-fopenmp', '-lgfortran'],
	cpp_args: '-fopenmp'
)
