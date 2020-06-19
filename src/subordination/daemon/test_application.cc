#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/daemon/test_application.hh>
#include <subordination/test/config.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("app", args...);
}

std::string failure = "no";
bool is_superior = false;

inline bool
test_without_failures() {
    return failure.empty() || failure == "no-failure";
}

inline bool
test_subordinate_failure() {
    return failure == "subordinate-failure";
}

inline bool
test_superior_failure() {
    return failure == "superior-failure";
}

inline bool
test_power_failure() {
    return failure == "total-failure";
}

class subordinate_kernel: public sbn::kernel {

private:
    uint32_t _number = 0;
    uint32_t _nsubordinates = 0;

public:

    subordinate_kernel() = default;

    explicit
    subordinate_kernel(uint32_t number, uint32_t nsubordinates):
    _number(number),
    _nsubordinates(nsubordinates)
    {}

    void act() override {
        if (!test_without_failures()) {
            using namespace sbn::this_application;
            if ((test_superior_failure() && is_superior) ||
                (test_subordinate_failure() && !is_superior) ||
                test_power_failure()) {
                // TODO move this logic to dtest
                log("kill _ at _ parent _", is_superior ? "superior" : "subordinate",
                    sys::this_process::hostname(), sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::id());
                sys::this_process::execute({SBN_TEST_EMPTY_EXE_PATH,0});
            }
        }
        log("subordinate act id _ number _ count _", id(), this->_number,
            this->_nsubordinates);
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_number << this->_nsubordinates;
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        in >> this->_number >> this->_nsubordinates;
    }

    inline uint32_t
    number() const noexcept {
        return this->_number;
    }

};

class superior_kernel: public sbn::kernel {

private:
    uint32_t _nkernels = SUBORDINATION_TEST_NUM_KERNELS;
    uint32_t _nreturned = 0;

public:

    superior_kernel() = default;
    ~superior_kernel() = default;

    void act() override {
        log("superior start");
        if (!test_superior_failure()) { is_superior = true; }
        for (uint32_t i=0; i<this->_nkernels; ++i) {
            auto subordinate = sbn::make_pointer<subordinate_kernel>(i+1, this->_nkernels);
            sbn::upstream<sbn::Remote>(this, std::move(subordinate));
        }
    }

    void react(sbn::kernel_ptr&& child) {
        auto k = sbn::pointer_dynamic_cast<subordinate_kernel>(std::move(child));
        ++this->_nreturned;
        if (k->source()) {
            log("superior react _ [_/_] from _", k->number(),
                this->_nreturned, this->_nkernels, k->source());
        } else {
            log("superior react _ [_/_]", k->number(), this->_nreturned, this->_nkernels);
        }
        if (this->_nreturned == this->_nkernels) {
            log("superior finish");
            if (test_without_failures()) {
                sbn::commit(std::move(this_ptr()));
            } else {
                sbn::commit<sbn::Remote>(std::move(this_ptr()));
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

class grand_superior_kernel: public sbn::kernel {

public:

    grand_superior_kernel() = default;

    ~grand_superior_kernel() = default;

    void
    act() override {
        log("grand start");
        if (test_superior_failure()) { is_superior = true; }
        auto superior = sbn::make_pointer<superior_kernel>();
        superior->setf(sbn::kernel_flag::carries_parent);
        sbn::upstream<sbn::Remote>(this, std::move(superior));
    }

    void
    react(sbn::kernel_ptr&& child) {
        log("grand finish");
        sbn::commit(std::move(this_ptr()));
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
    if (test_superior_failure()) {
        factory.types().add<grand_superior_kernel>(1);
        factory.types().add<superior_kernel>(2);
    } else {
        //factory.types().add<grand_superior_kernel>(2);
        factory.types().add<superior_kernel>(1);
    }
    factory.types().add<subordinate_kernel>(3);
    factory_guard g;
    return wait_and_return();
}
