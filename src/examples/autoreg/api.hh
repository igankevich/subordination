#ifndef EXAMPLES_AUTOREG_API_HH
#define EXAMPLES_AUTOREG_API_HH

#if defined(AUTOREG_MPI)
#include <subordination/core/kernel_buffer.hh>
namespace sbn {

    enum Target {
        Local,
        Remote,
    };

    template <Target t=Target::Local>
    inline void
    send(kernel_ptr&& k) {
        k->this_ptr(&k);
        k->act();
        k.release();
    }

    template<Target target=Target::Local>
    void
    upstream(kernel* lhs, kernel_ptr&& rhs) {
        rhs->parent(lhs);
        rhs->this_ptr(&rhs);
        rhs->act();
        rhs.release();
    }

    template<Target target=Target::Local>
    void
    commit(kernel_ptr&& k, exit_code ret) {
        if (!k->has_parent()) {
            k.reset();
        } else {
            k->return_to_parent(ret);
            auto* p = k->parent();
            sbn::kernel_ptr ptr(p);
            p->this_ptr(&ptr);
            p->react(std::move(k));
            ptr.release();
        }
    }

    template<Target target=Target::Local>
    void
    commit(kernel_ptr&& rhs) {
        exit_code ret = rhs->return_code();
        commit<target>(
            std::move(rhs),
            ret == exit_code::undefined ? exit_code::success :
            ret
        );
    }

}
#else
#include <subordination/api.hh>
#endif

#endif // vim:filetype=cpp
