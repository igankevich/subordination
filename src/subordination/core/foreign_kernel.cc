#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel_buffer.hh>

void sbn::foreign_kernel::write(kernel_buffer& out) const {
    out << this->_type;
    sbn::kernel::write(out);
    out.write(this->_payload.get(), this->_size);
}

void sbn::foreign_kernel::read(kernel_buffer& in) {
    in >> this->_type;
    sbn::kernel::read(in);
    this->_size = in.limit() - in.position();
    this->_payload.reset(new char[this->_size]);
    in.read(this->_payload.get(), this->_size);
}
