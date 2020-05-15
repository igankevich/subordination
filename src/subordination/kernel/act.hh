#ifndef SUBORDINATION_KERNEL_ACT_HH
#define SUBORDINATION_KERNEL_ACT_HH

#include <subordination/kernel/kernel.hh>
#include <subordination/ppl/basic_pipeline.hh>

namespace sbn {

    inline void
    act(kernel* k) {
        bool del = false;
        if (k->return_code() == exit_code::undefined) {
            if (k->principal()) {
                k->principal()->react(k);
                if (!k->isset(kernel_flag::do_not_delete)) {
                    del = true;
                } else {
                    k->unsetf(kernel_flag::do_not_delete);
                }
            } else {
                k->act();
            }
        } else {
            if (!k->principal()) {
                del = !k->parent();
            } else {
                del = *k->principal() == *k->parent();
                if (k->return_code() == exit_code::success) {
                    k->principal()->react(k);
                } else {
                    k->principal()->error(k);
                }
            }
        }
        if (del) {
            delete k;
        }
    }

}

#endif // vim:filetype=cpp
