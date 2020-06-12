#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/test_application.hh>
#include <subordination/test/config.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message(__FILE__, args...);
}

std::string failure = "no";
bool is_master = false;

inline bool
test_without_failures() {
    return failure.empty() || failure == "no-failure";
}

inline bool
test_slave_failure() {
    return failure == "slave-failure";
}

inline bool
test_master_failure() {
    return failure == "master-failure";
}

inline bool
test_total_failure() {
    return failure == "total-failure";
}

class slave_kernel: public sbn::kernel {

private:
    uint32_t _number = 0;
    uint32_t _nslaves = 0;

public:

    slave_kernel() = default;

    explicit
    slave_kernel(uint32_t number, uint32_t nslaves):
    _number(number),
    _nslaves(nslaves)
    {}

    void
    act() override {
        if (!test_without_failures()) {
            using namespace sbn::this_application;
            if ((test_master_failure() && is_master) ||
                (test_slave_failure() && !is_master) ||
                test_total_failure()) {
                log("kill _ at _ parent _", is_master ? "master" : "slave",
                    sys::this_process::hostname(), sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::parent_id());
                sys::this_process::execute({SBN_TEST_EMPTY_EXE_PATH,0});
            }
        }
        log("slave act id _ [_/_]", id(), this->_number, this->_nslaves);
        sbn::commit<sbn::Remote>(this);
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_number << this->_nslaves;
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        in >> this->_number >> this->_nslaves;
    }

    inline uint32_t
    number() const noexcept {
        return this->_number;
    }

};

class master_kernel: public sbn::kernel {

private:
    uint32_t _nkernels = SUBORDINATION_TEST_NUM_KERNELS;
    uint32_t _nreturned = 0;

public:

    master_kernel() = default;

    ~master_kernel() = default;

    void
    act() override {
        log("master start");
        if (!test_master_failure()) { is_master = true; }
        for (uint32_t i=0; i<this->_nkernels; ++i) {
            slave_kernel* slave = new slave_kernel(i+1, this->_nkernels);
            sbn::upstream<sbn::Remote>(this, slave);
        }
    }

    void
    react(sbn::kernel* child) {
        slave_kernel* k = dynamic_cast<slave_kernel*>(child);
        if (k->source()) {
            log("master react [_/_] from _", k->number(), this->_nkernels, k->source());
        } else {
            log("master react [_/_]", k->number(), this->_nkernels);
        }
        if (++this->_nreturned == this->_nkernels) {
            log("master finish");
            if (test_without_failures()) {
                sbn::commit(this);
            } else {
                sbn::commit<sbn::Remote>(this);
            }
        }
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
    }

};

class grand_master_kernel: public sbn::kernel {

public:

    grand_master_kernel() = default;

    ~grand_master_kernel() = default;

    void
    act() override {
        log("grand start");
        if (test_master_failure()) { is_master = true; }
        master_kernel* master = new master_kernel;
        master->setf(sbn::kernel_flag::carries_parent);
        sbn::upstream<sbn::Remote>(this, master);
    }

    void
    react(sbn::kernel* child) {
        log("grand finish");
        sbn::commit(this);
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
    }

};

int
main(int argc, char* argv[]) {
    if (argc >= 2) {
        failure = argv[1];
    }
    using namespace sbn;
    install_error_handler();
    if (test_master_failure()) {
        factory.types().add<grand_master_kernel>(1);
        factory.types().add<master_kernel>(2);
    } else {
        factory.types().add<grand_master_kernel>(2);
        factory.types().add<master_kernel>(1);
    }
    factory.types().add<slave_kernel>(3);
    factory_guard g;
    /*
    if (this_application::is_master()) {
        if (test_master_failure()) {
            send(new grand_master_kernel);
        } else {
            send(new master_kernel);
        }
    }
    */
    return wait_and_return();
}
