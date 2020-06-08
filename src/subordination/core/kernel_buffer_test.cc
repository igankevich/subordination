#include <gtest/gtest.h>

#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_type_registry.hh>

TEST(socket_address, _) {
    sys::socket_address inputs[] {
        sys::socket_address{},
        sys::socket_address{{127,0,0,1},2222},
        sys::socket_address{{127,0,0,1},0},
        sys::socket_address{{84,10,32,12},321},
        sys::socket_address{"\0/tmp/.socket"},
        sys::socket_address{"/tmp/.socket"},
        sys::socket_address{{0,0,0,0xffff,0x127,1,2,3},333},
    };
    for (const auto& a : inputs) {
        sys::socket_address b;
        sbn::kernel_buffer buf;
        buf.write(a);
        buf.flip();
        buf.read(b);
        EXPECT_EQ(a, b);
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
}

TEST(interface_address, ipv4) {
    using interface_ipv4_address = sys::interface_address<sys::ipv4_address>;
    interface_ipv4_address inputs[] {
        interface_ipv4_address{},
        interface_ipv4_address{{127,0,0,1},{255,0,0,0}},
    };
    for (const auto& a : inputs) {
        interface_ipv4_address b;
        sbn::kernel_buffer buf;
        buf.write(a);
        buf.flip();
        buf.read(b);
        EXPECT_EQ(a, b);
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
}

TEST(interface_address, ipv6) {
    using interface_ipv6_address = sys::interface_address<sys::ipv6_address>;
    interface_ipv6_address inputs[] {
        interface_ipv6_address{},
        interface_ipv6_address{{0,0,0,0xffff,0x127,1,2,3},{255,0,0,0,0,0,0,0}},
    };
    for (const auto& a : inputs) {
        interface_ipv6_address b;
        sbn::kernel_buffer buf;
        buf.write(a);
        buf.flip();
        buf.read(b);
        EXPECT_EQ(a, b);
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
}

TEST(frame, _) {
    sys::u32 x = 123, y;
    sbn::kernel_buffer buf;
    {
        sbn::kernel_frame frame;
        sbn::kernel_write_guard g(frame, buf);
        buf.write(x);
    }
    buf.flip();
    {
        sbn::kernel_frame frame;
        sbn::kernel_read_guard g(frame, buf);
        EXPECT_TRUE(g);
        buf.read(y);
        EXPECT_EQ(x, y);
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
    buf.compact();
    EXPECT_EQ(0u, buf.position());
}

TEST(frame, empty) {
    sbn::kernel_buffer buf;
    {
        sbn::kernel_frame frame;
        sbn::kernel_write_guard g(frame, buf);
    }
    EXPECT_EQ(0u, buf.position());
    buf.flip();
    EXPECT_EQ(0u, buf.position());
    EXPECT_EQ(0u, buf.limit());
    {
        sbn::kernel_frame frame;
        sbn::kernel_read_guard g(frame, buf);
        EXPECT_FALSE(g);
        EXPECT_EQ(0u, buf.position());
        EXPECT_EQ(0u, buf.limit());
    }
    buf.compact();
    EXPECT_EQ(0u, buf.position());
}

class Test_kernel: public sbn::kernel {
private:
    sys::u32 _number = 0;
public:
    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_number;
    }
    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        in >> this->_number;
    }
    inline const sys::u32& number() const noexcept { return this->_number; }
    inline void number(const sys::u32& rhs) noexcept { this->_number = rhs; }
};

TEST(kernel, bare) {
    Test_kernel a, b;
    a.number(123);
    sbn::kernel_buffer buf;
    {
        sbn::kernel_frame frame;
        sbn::kernel_write_guard g(frame, buf);
        a.write_header(buf);
        a.write(buf);
    }
    buf.flip();
    {
        sbn::kernel_frame frame;
        sbn::kernel_read_guard g(frame, buf);
        EXPECT_TRUE(g);
        b.read_header(buf);
        b.read(buf);
        EXPECT_EQ(a.number(), b.number());
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
    buf.compact();
    EXPECT_EQ(0u, buf.position());
}

TEST(kernel, with_type) {
    sbn::kernel_type_registry types;
    types.add<Test_kernel>(333);
    sbn::foreign_kernel f;
    Test_kernel a;
    a.number(123);
    sbn::kernel_buffer buf;
    buf.types(&types);
    {
        sbn::kernel_frame frame;
        sbn::kernel_write_guard g(frame, buf);
        a.write_header(buf);
        buf.write(&a);
    }
    buf.flip();
    {
        sbn::kernel_frame frame;
        sbn::kernel_read_guard g(frame, buf);
        EXPECT_TRUE(g);
        f.read_header(buf);
        f.read(buf);
        EXPECT_EQ(buf.limit(), buf.position());
        char tmp;
        EXPECT_THROW(buf.read(tmp), std::range_error);
    }
    buf.compact();
    EXPECT_EQ(0u, buf.position());
}
