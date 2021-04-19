#include <gtest/gtest.h>

#include <subordination/core/resources.hh>

std::string to_string(const sbn::resources::expression_ptr& expr) {
    std::stringstream tmp;
    tmp << *expr;
    return tmp.str();
}

TEST(resources, io_textual) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = r::total_threads != 2u && r::hostname == "2";
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

TEST(resources, bad_symbol_names) {
    EXPECT_THROW(sbn::resources::read("(= bad() 0)", 10), std::invalid_argument);
    EXPECT_THROW(sbn::resources::read("(= 1bad 0)", 10), std::invalid_argument);
    EXPECT_NO_THROW(sbn::resources::read("(= good 0)", 10));
    EXPECT_NO_THROW(sbn::resources::read("(= -good- 0)", 10));
    EXPECT_NO_THROW(sbn::resources::read("(= %good% 0)", 10));
    EXPECT_NO_THROW(sbn::resources::read("(= .good. 0)", 10));
}

TEST(resources, io_binary) {
    using namespace sbn::resources;
    using r = resources;
    auto expected = (r::total_threads*2u != 2u && r::hostname == "hello");
    sys::byte_buffer buf;
    expected->write(buf);
    buf.flip();
    auto actual = sbn::resources::read(buf);
    EXPECT_EQ(to_string(expected), to_string(actual));
}

TEST(bindings, equal) {
    using namespace sbn::resources;
    using r = resources;
    Bindings orig;
    Bindings b;
    EXPECT_EQ(orig, b);
    b["x"] = 1u;
    EXPECT_NE(orig, b);
    b.unset("x");
    EXPECT_EQ(orig, b);
    b[r::hostname] = "hello";
    EXPECT_NE(orig, b);
}

TEST(bindings, io_binary) {
    using namespace sbn::resources;
    using r = resources;
    Bindings expected;
    expected[r::hostname] = "hello";
    expected["x"] = 10u;
    sys::byte_buffer buf;
    expected.write(buf);
    buf.flip();
    Bindings actual;
    actual.read(buf);
    EXPECT_EQ(expected, actual);
}

TEST(bindings, io_binary_empty_string) {
    using namespace sbn::resources;
    using r = resources;
    Bindings expected;
    expected[r::hostname] = "";
    expected["x"] = 10u;
    sys::byte_buffer buf;
    expected.write(buf);
    buf.flip();
    Bindings actual;
    actual.read(buf);
    EXPECT_EQ(expected, actual);
}
