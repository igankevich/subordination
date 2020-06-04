#include <memory>
#include <vector>

#include <subordination/core/kernel_protocol.hh>

namespace  {

    inline void send_or_delete(sbn::pipeline* ppl, sbn::kernel* k, bool& remove) {
        if (ppl) { ppl->send(k); }
        else { remove = true; }
    };

    inline void forward_or_delete(sbn::pipeline* ppl, sbn::kernel* k, bool& remove) {
        if (ppl) { ppl->forward(dynamic_cast<sbn::foreign_kernel*>(k)); }
        else { remove = true; }
    };
}

void sbn::kernel_protocol::clear() {
    std::vector<std::unique_ptr<kernel>> sack;
    for (auto* k : this->_upstream) { k->mark_as_deleted(sack); }
    this->_upstream.clear();
    for (auto* k : this->_downstream) { k->mark_as_deleted(sack); }
    this->_downstream.clear();
}

void sbn::kernel_protocol::recover_kernel(kernel* k) {
    bool remove = false;
    #if defined(SBN_DEBUG)
    this->log("try to recover _", k->id());
    #endif
    const bool native = k->is_native();
    if (k->moves_upstream() && !k->to()) {
        #if defined(SBN_DEBUG)
        this->log("recover _", *k);
        #endif
        if (native) {
            send_or_delete(this->_remote_pipeline, k, remove);
        } else {
            forward_or_delete(this->_remote_pipeline, k, remove);
        }
    } else if (k->moves_somewhere() || (k->moves_upstream() && k->to())) {
        #if defined(SBN_DEBUG)
        this->log("destination is unreachable for _", *k);
        #endif
        k->from(k->to());
        k->return_code(exit_code::endpoint_not_connected);
        k->principal(k->parent());
        if (native) {
            send_or_delete(this->_native_pipeline, k, remove);
        } else {
            forward_or_delete(this->_foreign_pipeline, k, remove);
        }
    } else if (k->moves_downstream() && k->carries_parent()) {
        #if defined(SBN_DEBUG)
        this->log("restore parent _", *k);
        #endif
        if (native) {
            send_or_delete(this->_native_pipeline, k, remove);
        } else {
            forward_or_delete(this->_foreign_pipeline, k, remove);
        }
    } else {
        remove = true;
    }
    if (remove) {
        log("delete _", *k);
        delete k;
    }
}
