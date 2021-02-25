#include <valgrind/config.hh>

#include <unistdx/ipc/execute>

int main(int argc, char* argv[]) {
    #if defined(SBN_TEST_HAVE_VALGRIND_H)
    if (RUNNING_ON_VALGRIND) { std::exit(77); }
    #endif
    if (argc != 3) { std::exit(1); }
    const char* sbn_python = argv[1];
    const char* test_py = argv[2];
    sys::argstream args;
    args.append(sbn_python);
    args.append(test_py);
    sys::this_process::execute(args);
}
