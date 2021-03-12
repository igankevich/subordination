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
        log("subordinate act id _ number _ count _",
            id(), this->_number, this->_nsubordinates);
        log("subordinate hostname _", sys::this_process::hostname());
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
    uint32_t _nkernels = 10;
    uint32_t _nreturned = 0;

public:

    superior_kernel() = default;
    ~superior_kernel() = default;

    void act() override {
        log("superior start");
        for (uint32_t i=0; i<this->_nkernels; ++i) {
            auto subordinate = sbn::make_pointer<subordinate_kernel>(i+1, this->_nkernels);
            using namespace sbn::resources;
            subordinate->node_filter(expression_ptr(new Name("tag")) == 2u);
            sbn::upstream<sbn::Remote>(this, std::move(subordinate));
        }
    }

    void react(sbn::kernel_ptr&& child) {
        auto k = sbn::pointer_dynamic_cast<subordinate_kernel>(std::move(child));
        ++this->_nreturned;
        if (k->source()) {
            log("superior react _ [_/_] from _ result _", k->number(),
                this->_nreturned, this->_nkernels, k->source(),
                k->return_code());
        } else {
            log("superior react _ [_/_] result _",
                k->number(), this->_nreturned, this->_nkernels, k->return_code());
        }
        if (this->_nreturned == this->_nkernels) {
            log("superior finish");
            sbn::commit(std::move(this_ptr()));
        }
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
    }

};

int main(int argc, char* argv[]) {
    using namespace sbn;
    install_error_handler();
    factory.types().add<superior_kernel>(1);
    factory.types().add<subordinate_kernel>(2);
    factory_guard g;
    return wait_and_return();
}
