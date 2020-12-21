#include <gtest/gtest.h>

#include <subordination/core/resources.hh>

TEST(resources, io) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = r::total_threads != 2u;
    std::clog << *expected << std::endl;
    std::stringstream tmp;
    tmp << *expected;
    auto actual = sbn::resources::read(tmp, 10);
    std::clog << *actual << std::endl;
    std::stringstream tmp2;
    tmp2 << *actual;
    EXPECT_EQ(tmp.str(), tmp2.str());
}

TEST(resources, string) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = r::hostname != "x";
    std::clog << *expected << std::endl;
    std::stringstream tmp;
    tmp << *expected;
    auto actual = sbn::resources::read(tmp, 10);
    std::clog << *actual << std::endl;
    std::stringstream tmp2;
    tmp2 << *actual;
    EXPECT_EQ(tmp.str(), tmp2.str());
}

TEST(resources, string_escape) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = r::hostname != "\"x\"";
    std::clog << *expected << std::endl;
    std::stringstream tmp;
    tmp << *expected;
    auto actual = sbn::resources::read(tmp, 10);
    std::clog << *actual << std::endl;
    std::stringstream tmp2;
    tmp2 << *actual;
    EXPECT_EQ(tmp.str(), tmp2.str());
}

TEST(resources, name_io) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = r::hostname != expression_ptr(new Name("x"));
    std::clog << *expected << std::endl;
    std::stringstream tmp;
    tmp << *expected;
    auto actual = sbn::resources::read(tmp, 10);
    std::clog << *actual << std::endl;
    std::stringstream tmp2;
    tmp2 << *actual;
    EXPECT_EQ(tmp.str(), tmp2.str());
}

TEST(resources, name_evaluate) {
    using namespace sbn::resources;
    using r = resources;
    auto expr = r::hostname == expression_ptr(new Name("x"));
    std::clog << *expr << std::endl;
    Bindings bindings;
    bindings[r::hostname] = "hello";
    bindings["x"] = "hello";
    EXPECT_TRUE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
    bindings["x"] = "helloXXX";
    EXPECT_FALSE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
    bindings.unset("x");
    EXPECT_FALSE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
}

TEST(resources, name_does_not_override_resource) {
    using namespace sbn::resources;
    using r = resources;
    auto expr = sbn::resources::read("(= hostname \"hello\")", 10);
    std::clog << *expr << std::endl;
    Bindings bindings;
    EXPECT_FALSE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
    bindings["hostname"] = "hello";
    EXPECT_FALSE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
    bindings[r::hostname] = "hello";
    EXPECT_TRUE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
    bindings.unset("hostname");
    EXPECT_TRUE(expr->evaluate(bindings).boolean()) << "Code:\n" << bindings << *expr;
}
