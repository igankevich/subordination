#include <gtest/gtest.h>

#include <subordination/guile/base.hh>
#include <subordination/guile/init.hh>

using namespace sbn::guile;

SCM eval(const char* s) {
    return scm_c_eval_string(s);
}

TEST(protected_scm, copy_contruct) {
    protected_scm a(eval("(+ 1 2)"));
    protected_scm b(a);
}

TEST(protected_scm, move_contruct) {
    protected_scm a(eval("(+ 1 2)"));
    protected_scm b(std::move(a));
}

TEST(protected_scm, copy_assign) {
    protected_scm a(eval("(+ 1 2)"));
    protected_scm b(eval("(+ 3 4)"));
    a = b;
}

TEST(protected_scm, move_assign) {
    protected_scm a(eval("(+ 1 2)"));
    protected_scm b(eval("(+ 3 4)"));
    a = std::move(b);
}

void nested_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    std::exit(RUN_ALL_TESTS());
}

int main(int argc, char* argv[]) {
    sbn::guile::main(argc, argv, nested_main);
    return 0;
}
