#include <subordination/daemon/factory.hh>
#include <subordination/daemon/main_kernel.hh>
#include <subordination/daemon/transaction_test_kernel.hh>

void sbnd::Transaction_test_kernel::act() {}
void sbnd::Transaction_test_kernel::react(sbn::kernel_ptr&& k) {}

void sbnd::Transaction_test_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_application << this->_exit_codes;
}

void sbnd::Transaction_test_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_application >> this->_exit_codes;
}
