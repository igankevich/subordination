#include <subordination/core/kernel_protocol.hh>

sbn::kernel_protocol::~kernel_protocol() {
    for (auto* k : this->_upstream) { delete k; } this->_upstream.clear();
    for (auto* k : this->_downstream) { delete k; } this->_downstream.clear();
}
