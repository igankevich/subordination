#ifndef SUBORDINATION_PPL_BASIC_ROUTER_HH
#define SUBORDINATION_PPL_BASIC_ROUTER_HH

#include <subordination/kernel/foreign_kernel.hh>
#include <subordination/ppl/application.hh>

namespace sbn {

    template <class T>
    struct basic_router {

        typedef T kernel_type;

        static void
        send_local(T* rhs);

        static void
        send_remote(T*);

        static void
        forward(foreign_kernel* hdr);

        static void
        forward_child(foreign_kernel* hdr);

        static void
        forward_parent(foreign_kernel* hdr);

        static void
        execute(const application& app);

    };

}

#endif // vim:filetype=cpp
