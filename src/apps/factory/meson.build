flags2 = ['-static-libgcc', '-static-libstdc++', '-static', '-lpthread', '-lm']
if host_machine.system() == 'darwin'	
	flags2 = ['-lpthread', '-lm']
endif
executable(
	'factory',
	sources: 'factory.cc',
	include_directories: [srcdir, testdir],
	link_args: flags2
)
