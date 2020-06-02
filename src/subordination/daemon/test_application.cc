#include <subordination/api.hh>
#include <subordination/base/error_handler.hh>
#include <subordination/daemon/test_application.hh>
#include <test/config.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message(__FILE__, args...);
}

std::string failure = "no";

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
            if ((test_master_failure() && is_master()) ||
                (test_slave_failure() && is_slave())) {
                log(
                    "kill _ at _ parent _",
                    is_master() ? "master" : "slave",
                    sys::this_process::hostname(),
                    sys::this_process::parent_id()
                );
                send(sys::signal::kill, sys::this_process::parent_id());
                sys::this_process::execute({SBN_TEST_EMPTY_EXE_PATH,0});
            }
        }
        log("slave act id _ [_/_]", id(), this->_number, this->_nslaves);
        sbn::commit<sbn::Remote>(this);
    }

    void
    write(sys::pstream& out) const override {
        sbn::kernel::write(out);
        out << this->_number << this->_nslaves;
    }

    void
    read(sys::pstream& in) override {
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
        for (uint32_t i=0; i<this->_nkernels; ++i) {
            slave_kernel* slave = new slave_kernel(i+1, this->_nkernels);
            sbn::upstream<sbn::Remote>(this, slave);
        }
    }

    void
    react(sbn::kernel* child) {
        slave_kernel* k = dynamic_cast<slave_kernel*>(child);
        if (k->from()) {
            log("master react [_/_] from _", k->number(), this->_nkernels, k->from());
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

    void
    write(sys::pstream& out) const override {
        sbn::kernel::write(out);
    }

    void
    read(sys::pstream& in) override {
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
        master_kernel* master = new master_kernel;
        master->setf(sbn::kernel_flag::carries_parent);
        sbn::upstream<sbn::Remote>(this, master);
    }

    void
    react(sbn::kernel* child) {
        log("grand finish");
        sbn::commit(this);
    }

    void
    write(sys::pstream& out) const override {
        sbn::kernel::write(out);
    }

    void
    read(sys::pstream& in) override {
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
    register_type<slave_kernel>(1);
    register_type<master_kernel>(2);
    register_type<grand_master_kernel>(3);
    factory_guard g;
    if (this_application::is_master()) {
        if (test_master_failure()) {
            send(new grand_master_kernel);
        } else {
            send(new master_kernel);
        }
    }
    return wait_and_return();
}
