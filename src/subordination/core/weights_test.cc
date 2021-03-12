#include <gtest/gtest.h>

#include <subordination/core/weights.hh>

constexpr const auto max = std::numeric_limits<sys::u32>::max();

TEST(weight, underflow) {
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-1u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-2u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-3u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-std::numeric_limits<sys::u32>::max());
}

TEST(weight, underflow_member_functions) {
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-=1u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-=2u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-=3u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}-=std::numeric_limits<sys::u32>::max());
}

TEST(weight, overflow) {
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+1u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+2u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+3u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+max);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*1u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*2u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*3u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*4u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*5u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*6u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*7u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*8u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*9u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*10u);
}

TEST(weight, overflow_member_functions) {
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+=1u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+=2u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+=3u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}+=max);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=1u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=2u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=3u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=4u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=5u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=6u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=7u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=8u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=9u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{max}*=10u);
}

TEST(weight, division) {
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{1}/0u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{0}/0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{1}%0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}%0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}/max);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{1}/max);
}

TEST(weight, division_member_functions) {
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{1}/=0u);
    EXPECT_EQ(sbn::weight_type{max}, sbn::weight_type{0}/=0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{1}%=0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}%=0u);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{0}/=max);
    EXPECT_EQ(sbn::weight_type{0}, sbn::weight_type{1}/=max);
}
