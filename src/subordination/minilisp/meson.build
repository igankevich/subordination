sbn_minilisp_src = files([
    'minilisp.cc',
])

clang_tidy_files += sbn_minilisp_src

sbn_minilisp_deps = [sbn,threads,unistdx]

sbn_minilisp_lib = library(
    'sbn-minilisp',
    sources: sbn_minilisp_src,
    dependencies: sbn_minilisp_deps,
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

sbn_minilisp_exe = executable(
    'sbn-minilisp',
    sources: ['main.cc'],
    dependencies: sbn_minilisp_deps,
    include_directories: src,
    install: true,
    implicit_include_directories: false,
    link_with: sbn_minilisp_lib
)

foreach name : ['read_write']
    test_name = '-'.join(name.split('_'))
    exe_name = test_name + '-test'
    exe = executable(
        exe_name,
        sources: [name + '_test.cc'],
        cpp_args: test_cpp_args,
        include_directories: src,
        dependencies: [sbnd, gtest],
        implicit_include_directories: false,
        link_with: sbn_minilisp_lib,
    )
    test('minilisp/' + test_name, exe)
endforeach
