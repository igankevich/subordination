#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/mobile_kernel.hh>

void sbn::mobile_kernel::read(kernel_buffer& in) { in >> _result >> _id; }
void sbn::mobile_kernel::write(kernel_buffer& out) const { out << _result << _id; }
