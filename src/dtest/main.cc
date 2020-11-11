#include <iostream>

#include <dtest/application.hh>

#include <valgrind/config.hh>

#if defined(SBN_TEST_HAVE_VALGRIND_H)
#include <valgrind.h>
#endif

int main(int argc, char* argv[]) {
    #if defined(SBN_TEST_HAVE_VALGRIND_H)
    if (RUNNING_ON_VALGRIND) { std::exit(77); }
    #endif
    int ret = 0;
    dts::application app;
    try {
        app.init(argc, argv);
        ret = run(app);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        app.terminate();
        ret = 1;
    }
    return ret;
}
