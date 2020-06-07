#include <gtest/gtest.h>

#include <unistdx/base/command_line>
#include <unistdx/ipc/process>
#include <unistdx/net/interface_address>

#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/daemon/socket_pipeline.hh>
#include <subordination/test/datum.hh>
#include <subordination/test/role.hh>

using test::Role;
using sys::this_process::hostname;

enum struct Failure: sys::port_type {
    No = 0,
    Slave = 1
};

std::istream&
operator>>(std::istream& in, Failure& rhs) {
    std::string s;
    in >> s;
    if (s == "slave") {
        rhs = Failure::Slave;
    } else if (s == "no") {
        rhs = Failure::No;
    } else {
        throw std::invalid_argument("bad test case");
    }
    return in;
}

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
//const std::vector<size_t> POWERS = {1,2,3,4};
const std::vector<size_t> POWERS = {1};
//const std::vector<size_t> POWERS = {16,17};
const uint32_t NUM_KERNELS = 7;

std::atomic<int> kernel_count(0);
std::atomic<uint32_t> shutdown_counter(0);

Role role = Role::Master;
Failure failure = Failure::No;

using ipv4_interface_address = sys::interface_address<sys::ipv4_address>;


sbn::parallel_pipeline local{1};
sbnd::socket_pipeline remote;

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("test", args ...);
}

struct Test_socket: public sbn::kernel {

    Test_socket():
    _data()
    { ++kernel_count; }

    explicit
    Test_socket(const std::vector<Datum>& x):
    _data(x)
    { ++kernel_count; }

    virtual
    ~Test_socket() {
        --kernel_count;
    }

    void act() override {
        message("act _", sys::this_process::hostname());
        //std::clog << "Test_socket::act()" << std::endl;
        if (failure == Failure::Slave) {
            if (role == Role::Slave) {
                // Delete kernel for Valgrind memory checker.
                delete this;
                const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();
                if (++shutdown_counter == TOTAL_NUM_KERNELS/3) {
                    std::clog << "go offline" << std::endl;
                    sbn::graceful_shutdown(0);
                }
            } else {
                return_to_parent(sbn::exit_code::success);
                local.send(this);
            }
        } else {
            return_to_parent(sbn::exit_code::success);
            remote.send(this);
        }
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << uint32_t(_data.size());
        for (size_t i=0; i<_data.size(); ++i)
            out << _data[i];
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        uint32_t sz;
        in >> sz;
        _data.resize(sz);
        for (size_t i=0; i<_data.size(); ++i)
            in >> _data[i];
    }

    std::vector<Datum> data() const {
        return _data;
    }

private:
    std::vector<Datum> _data;
};

struct Sender: public sbn::kernel {

    explicit
    Sender(uint32_t n):
    _vector_size(n),
    _input(_vector_size)
    {}

    void act() override {
        for (uint32_t i=0; i<NUM_KERNELS; ++i) {
            auto* k = new Test_socket(_input);
            k->parent(this);
            remote.send(k);
        }
    }

    void react(sbn::kernel* child) override {
        auto* test_kernel = dynamic_cast<Test_socket*>(child);
        std::vector<Datum> output = test_kernel->data();
        EXPECT_EQ(this->_input.size(), output.size());
        EXPECT_EQ(this->_input, output);
        message("returned _/_", _num_returned+1, NUM_KERNELS);
        if (++_num_returned == NUM_KERNELS) {
            this->return_to_parent(sbn::exit_code::success);
            local.send(this);
        }
    }

private:

    uint32_t _num_returned = 0;
    uint32_t _vector_size;

    std::vector<Datum> _input;
};

struct Main: public sbn::kernel {

    void
    act() override {
        for (uint32_t i=0; i<POWERS.size(); ++i) {
            size_t sz = 1 << POWERS[i];
            auto* sender = new Sender(sz);
            sender->parent(this);
            local.send(sender);
        }
    }

    void
    react(sbn::kernel*) override {
        if (++_num_returned == POWERS.size()) {
            return_code(sbn::exit_code::success);
            sbn::graceful_shutdown(this);
        }
    }

private:

    uint32_t _num_returned = 0;

};

TEST(NICServerTest, All) {

    sbn::kernel_type_registry types;
    types.add<Test_socket>(1);
    local.name("local");
    local.start();
    remote.name("remote");
    remote.native_pipeline(&local);
    remote.remote_pipeline(&remote);
    remote.types(&types);
    remote.start();

    sys::port_type port = 10000 + 2*sys::port_type(failure);
    ipv4_interface_address network{{127,0,0,1},8};
    if (const char* text = std::getenv("DTEST_INTERFACE_ADDRESS")) {
        std::stringstream tmp(text);
        tmp >> network;
    }
    auto address = network.begin();
    sys::socket_address subordinate_endpoint(*address++, port+1);
    sys::socket_address principal_endpoint(*address++, port);
    if (role == Role::Slave) {
        remote.set_port(port+1);
        remote.add_server(principal_endpoint, network.netmask());
    }
    if (role == Role::Master) {
        remote.set_port(port);
        remote.add_server(subordinate_endpoint, network.netmask());
        // wait for the child to start
        using namespace std::this_thread;
        using namespace std::chrono;
        sleep_for(milliseconds(1000));
        remote.add_client(principal_endpoint);
        local.send(new Main);
    }

    int retval = sbn::wait_and_return();

    if (!(failure == Failure::Slave && role == Role::Slave)) {
        EXPECT_EQ(0, kernel_count) << "some kernels were not deleted"
            " or were deleted multiple times";
    }
    EXPECT_EQ(0, retval);
    local.stop();
    remote.stop();
    local.wait();
    message("finished");
    remote.wait();
    local.clear();
    remote.clear();
}

int
main(int argc, char* argv[]) {
    sbn::install_error_handler();
    ::testing::InitGoogleTest(&argc, argv);
    sys::this_process::ignore_signal(sys::signal::broken_pipe);
    sys::input_operator_type options[] = {
        sys::ignore_first_argument(),
        sys::make_key_value("role", role),
        sys::make_key_value("failure", failure),
        nullptr
    };
    sys::parse_arguments(argc, argv, options);
    return RUN_ALL_TESTS();
}
