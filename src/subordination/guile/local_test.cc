#include <valgrind/config.hh>

#include <unistdx/ipc/execute>

int main(int argc, char* argv[]) {
    SBN_SKIP_IF_RUNNING_ON_VALGRIND();
    if (argc != 3) { std::exit(1); }
    const char* sbn_guile = argv[1];
    const char* test_scm = argv[2];
    sys::argstream args;
    args.append(sbn_guile);
    args.append(test_scm);
    sys::this_process::execute(args);
}
