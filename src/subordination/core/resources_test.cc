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
