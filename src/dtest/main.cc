#include <iostream>

#include <dtest/application.hh>

int main(int argc, char* argv[]) {
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
