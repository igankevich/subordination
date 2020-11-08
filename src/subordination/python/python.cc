#include <cstdio>
#include <vector>

#include <subordination/python/python.hh>

void sbn::python::set_arguments(int argc, char** argv) {
    std::vector<pointer<wchar_t>> tmp;
    tmp.reserve(argc-1);
    std::vector<wchar_t*> args;
    args.reserve(argc-1);
    for (int i=1; i<argc; ++i) {
        tmp.emplace_back(Py_DecodeLocale(argv[i], nullptr));
        args.emplace_back(tmp.back().get());
    }
    ::PySys_SetArgvEx(args.size(), args.data(), 0);
}

void sbn::python::load(const char* filename) {
    auto fp = std::fopen(filename, "rb");
    ::PyCompilerFlags flags{};
    ::PyRun_SimpleFileExFlags(fp, filename, true, &flags);
}
