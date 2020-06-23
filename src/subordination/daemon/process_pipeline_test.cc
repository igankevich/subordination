#include <atomic>

#include <subordination/core/child_process_pipeline.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/daemon/process_pipeline.hh>
#include <subordination/daemon/process_pipeline_test.hh>
#include <subordination/test/config.hh>
#include <subordination/test/datum.hh>

const uint32_t NUM_SIZES = 10;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

sbn::parallel_pipeline local{1};
#if defined(SUBORDINATION_TEST_DAEMON)
sbnd::process_pipeline process;
#endif
#if defined(SUBORDINATION_TEST_APPLICATION)
sbn::child_process_pipeline remote;
#endif

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("test", args ...);
}

class Test_socket: public sbn::kernel {

private:
    std::vector<Datum> _data;

public:
    Test_socket():
    _data() {}

    explicit Test_socket(const std::vector<Datum>& x):
    _data(x) {
        ++kernel_count;
    }

    virtual ~Test_socket() {
        --kernel_count;
    }

    void
    act() override {
        message("Test_socket::act(): It works!");
        return_to_parent(sbn::exit_code::success);
        #if defined(SUBORDINATION_TEST_APPLICATION)
        remote.send(std::move(this_ptr()));
        #endif
    }

    void
    write(sbn::kernel_buffer& out) const override {
        message("Test_socket::write()");
        kernel::write(out);
        out << uint32_t(_data.size());
        for (size_t i=0; i<_data.size(); ++i)
            out << _data[i];
    }

    void
    read(sbn::kernel_buffer& in) override {
        message("Test_socket::read()");
        kernel::read(in);
        uint32_t sz;
        in >> sz;
        _data.resize(sz);
        for (size_t i=0; i<_data.size(); ++i)
            in >> _data[i];
    }

    std::vector<Datum>
    data() const {
        return _data;
    }

};

class Main: public sbn::kernel {

private:
    uint32_t _num_returned = 0;

public:

    void act() override {
        for (uint32_t i=1; i<=NUM_SIZES; ++i) {
            message("sent _/_", i, NUM_SIZES);
            auto k = sbn::make_pointer<Test_socket>();
//			k->setapp(sys::this_process::id());
            k->parent(this);
            #if defined(SUBORDINATION_TEST_APPLICATION)
            remote.send(std::move(k));
            #endif
        }
    }

    void react(sbn::kernel_ptr&&) override {
        message("returned _/_", _num_returned+1, NUM_SIZES);
        if (++_num_returned == NUM_SIZES) {
            message("finished");
            return_to_parent(sbn::exit_code::success);
            local.send(std::move(this_ptr()));
        }
    }

};

void terminate_test(int) { sbn::exit(0); }

#if defined(SUBORDINATION_TEST_DAEMON)
int main(int argc, char* argv[]) {
    sbn::install_error_handler();
    sys::this_process::bind_signal(sys::signal::user_defined_1, terminate_test);
    sbn::application app({SBN_TEST_APP_EXE_PATH}, {});
    process.name("process");
    process.native_pipeline(&local);
    process.foreign_pipeline(&process);
    process.remote_pipeline(nullptr);
    process.start();
    process.add(app);
    auto ret = sbn::wait_and_return();
    message("daemon returned _", ret);
    process.stop();
    process.wait();
    local.stop();
    local.wait();
    sbn::kernel_sack sack;
    process.clear(sack);
    local.clear(sack);
    return ret;
}
#endif

#if defined(SUBORDINATION_TEST_APPLICATION)
int main(int argc, char* argv[]) {
    sbn::install_error_handler();
    sbn::kernel_type_registry types;
    types.add<Test_socket>(1);
    remote.types(&types);
    remote.native_pipeline(&local);
    remote.foreign_pipeline(&remote);
    remote.remote_pipeline(&remote);
    remote.start();
    local.name("local");
    local.start();
    local.send(sbn::make_pointer<Main>());
    auto ret = sbn::wait_and_return();
    message("application returned _", ret);
    remote.stop();
    remote.wait();
    local.stop();
    local.wait();
    sbn::kernel_sack sack;
    remote.clear(sack);
    local.clear(sack);
    sys::send(sys::signal::user_defined_1, sys::this_process::parent_id());
    return ret;
}
#endif
