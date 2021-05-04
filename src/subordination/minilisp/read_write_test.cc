#include <gtest/gtest.h>

#include <subordination/core/kernel_buffer.hh>
#include <subordination/minilisp/minilisp.hh>

using namespace lisp;

void write_read_binary(Object* expected) {
    EXPECT_TRUE(expected);
    sbn::kernel_buffer buffer;
    buffer << expected;
    buffer.flip();
    Object* actual = ::lisp::read(buffer);
    EXPECT_TRUE(actual);
    EXPECT_TRUE(to_boolean(equal(expected, actual)))
        << "expected type: " << expected->type
        << "\nactual type:   " << actual->type
        << "\nexpected:      " << expected
        << "\nactual:        " << actual
        << "\nexpected address: " << std::addressof(expected)
        << "\nactual address: " << std::addressof(actual);
}

void read_write_textual(const char* expected) {
    lisp::string_view s(expected);
    auto obj = lisp::read(s);
    auto actual = to_string(obj);
    EXPECT_EQ(expected, actual);
}

TEST(read_write, binary) {
    write_read_binary(new Integer(123));
    write_read_binary(new Boolean(true));
    write_read_binary(new Boolean(false));
    write_read_binary(lisp::eval(nullptr, top_environment(), read("equal?")).object);
    write_read_binary(lisp::intern("hello"));
    write_read_binary(lisp::intern(""));
    write_read_binary(new Cell(intern("a"), intern("b")));
    write_read_binary(lisp::eval(nullptr, top_environment(), read("(lambda (x) 0)")).object);
    write_read_binary(top_environment());
}

TEST(read_write, textual) {
    read_write_textual("123");
    read_write_textual("#t");
    read_write_textual("#f");
    read_write_textual("(1)");
    read_write_textual("(1 2 3)");
    read_write_textual("hello");
    read_write_textual("(1 . 2)");
    read_write_textual("(lambda (x) 0)");
    read_write_textual(".");
}

int main(int argc, char* argv[]) {
    lisp::init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
