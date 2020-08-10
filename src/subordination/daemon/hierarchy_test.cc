#include <gtest/gtest.h>

#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy.hh>

TEST(hierarchy, read_write) {
    sbn::kernel_buffer buf;
    sbnd::Hierarchy<sys::ipv4_address> hier{{{127,0,0,1},24}, 33333};
    buf << hier;
    sbnd::Hierarchy<sys::ipv4_address> hier2;
    buf.flip();
    buf >> hier2;
    EXPECT_EQ(hier.interface_address(), hier2.interface_address());
    EXPECT_EQ(hier.socket_address(), hier2.socket_address());
}
