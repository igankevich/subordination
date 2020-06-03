#include <atomic>

#include <subordination/base/error_handler.hh>
#include <subordination/ppl/child_process_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#include <subordination/ppl/process_pipeline.hh>
#include <test/config.hh>

#include <test/datum.hh>

const uint32_t NUM_SIZES = 10;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

sbn::parallel_pipeline local{1};
sbn::process_pipeline process;
sbn::child_process_pipeline remote;

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
        remote.send(this);
    }

    void
    write(sys::pstream& out) const override {
        message("Test_socket::write()");
        kernel::write(out);
        out << uint32_t(_data.size());
        for (size_t i=0; i<_data.size(); ++i)
            out << _data[i];
    }

    void
    read(sys::pstream& in) override {
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
            auto* k = new Test_socket;
//			k->setapp(sys::this_process::id());
            k->parent(this);
            remote.send(k);
        }
    }

    void react(kernel*) override {
        message("returned _/_", _num_returned+1, NUM_SIZES);
        if (++_num_returned == NUM_SIZES) {
            message("finished");
            sbn::graceful_shutdown(0);
        }
    }

};


int
main(int argc, char* argv[]) {
    sbn::install_error_handler();
    sbn::types.register_type<Test_socket>(1);
    #if defined(SUBORDINATION_TEST_SERVER)
    sbn::application app({SBN_TEST_APP_EXE_PATH}, {});
    process.name("process");
    process.native_pipeline(&local);
    process.foreign_pipeline(&process);
    process.remote_pipeline(nullptr);
    process.add(app);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    sbn::graceful_shutdown(0);
    #else
    auto in = sbn::this_application::get_input_fd();
    auto out = sbn::this_application::get_output_fd();
    message("tst", "in = _, out = _", in, out);
    local.name("local");
    local.send(new Main);
    #endif
    return sbn::wait_and_return();
}
