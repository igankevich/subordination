#include <sstream>

#include <unistdx/ipc/process>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>

class Child: public sbn::kernel {

private:
    int _number = 0;
    int _value = 1;

public:
    inline Child() {}

    inline explicit Child(int number): _number(number) {}

    void act() override {
        std::stringstream msg;
        msg << "Hello from node " << sys::this_process::hostname();
        msg << ", child kernel number " << this->_number << '\n';
        std::clog << msg.str() << std::flush;
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_number;
        out << this->_value;
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        in >> this->_number;
        in >> this->_value;
    }

    inline int value() const noexcept { return this->_value; }

};

class Main: public sbn::kernel {

private:
    int _num_children = 10;
    int _sum = 0;

public:
    inline Main() {}

    inline Main(int argc, char* argv[]) {
        if (argc == 2) {
            this->_num_children = std::atoi(argv[1]);
        }
    }

    void act() override {
        if (this->_num_children == 0) {
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        } else {
            for (int i=0; i<this->_num_children; ++i) {
                sbn::upstream<sbn::Remote>(this, sbn::make_pointer<Child>(i+1));
            }
        }
    }

    void react(sbn::kernel_ptr&& k) {
        auto child = sbn::pointer_dynamic_cast<Child>(std::move(k));
        this->_sum += child->value();
        if (this->_sum == this->_num_children) {
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        }
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_num_children;
        out << this->_sum;
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        if (in.remaining() == 0) {
            if (auto* a = target_application()) {
                const auto& args = a->arguments();
                if (args.size() == 2) {
                    this->_num_children = std::stoi(args[1]);
                }
            }
        } else {
            in >> this->_num_children;
            in >> this->_sum;
        }
    }

};

void register_types() {
    using sbn::factory;
    auto g = factory.types().guard();
    factory.types().add<Main>(1);
    factory.types().add<Child>(2);
}

int main(int argc, char* argv[]) {
    using namespace sbn;
    register_types();
    factory_guard g;
    if (sbn::this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv));
    }
    return wait_and_return();
}
