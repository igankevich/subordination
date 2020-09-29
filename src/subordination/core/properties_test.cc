#include <iostream>
#include <sstream>
#include <unordered_map>

#include <gtest/gtest.h>

#include <subordination/core/properties.hh>

class my_properties: public sbn::properties {
public:
    std::unordered_map<std::string,std::string> all;
    void property(const std::string& key, const std::string& value) override {
        all[key] = value;
    }
};

TEST(properties, read_1) {
    my_properties props;
    std::stringstream in;
    in << R"(a=1
b = 2

# comment
c=3 # comment)";
    props.read(in, "string");
    EXPECT_EQ(props.all["a"], std::string("1"));
    EXPECT_EQ(props.all["b"], std::string("2"));
    EXPECT_EQ(props.all["c"], std::string("3"));
}

TEST(properties, read_2) {
    my_properties props;
    std::stringstream in;
    in << R"(a=1
b = 2

# comment
c=3)";
    props.read(in, "string");
    EXPECT_EQ(props.all["a"], std::string("1"));
    EXPECT_EQ(props.all["b"], std::string("2"));
    EXPECT_EQ(props.all["c"], std::string("3"));
}

TEST(properties, read_3) {
    my_properties props;
    const char* argv[] = {"", "a=1", "b=2", "c=3", 0};
    props.read(4, argv);
    EXPECT_EQ(props.all["a"], std::string("1"));
    EXPECT_EQ(props.all["b"], std::string("2"));
    EXPECT_EQ(props.all["c"], std::string("3"));
}

TEST(properties, read_4) {
    my_properties props;
    std::stringstream in;
    in << "a=1\r\nb=2\r\nc=3\r\n";
    props.read(in, "string");
    EXPECT_EQ(props.all["a"], std::string("1"));
    EXPECT_EQ(props.all["b"], std::string("2"));
    EXPECT_EQ(props.all["c"], std::string("3"));
}

TEST(properties, read_5) {
    my_properties props;
    std::stringstream in;
    in << "a=1\n\rb=2\n\rc=3\n\r";
    props.read(in, "string");
    EXPECT_EQ(props.all["a"], std::string("1"));
    EXPECT_EQ(props.all["b"], std::string("2"));
    EXPECT_EQ(props.all["c"], std::string("3"));
}
