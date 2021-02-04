#ifndef SUBORDINATION_API_HH
#define SUBORDINATION_API_HH

#include <subordination/core/factory.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/kernel_type_registry.hh>

namespace sbn {

    enum Target {
        Local,
        Remote,
    };

    template <Target t=Target::Local>
    inline void
    send(kernel_ptr&& k) {
        factory.local().send(std::move(k));
    }

    template <>
    inline void
    send<Local>(kernel_ptr&& k) {
        factory.local().send(std::move(k));
    }

    template <>
    inline void
    send<Remote>(kernel_ptr&& k) {
        factory.remote().send(std::move(k));
    }

    template<Target target=Target::Local>
    void
    upstream(kernel* lhs, kernel_ptr&& rhs) {
        rhs->parent(lhs);
        send<target>(std::move(rhs));
    }

    template<Target target=Target::Local>
    void
    commit(kernel_ptr&& rhs, exit_code ret) {
        if (!rhs->has_parent()) {
            rhs.reset();
            sbn::exit(static_cast<int>(ret));
        } else {
            rhs->return_to_parent(ret);
            send<target>(std::move(rhs));
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

    template<Target target=Target::Local>
    void
    send(kernel_ptr&& lhs, kernel* rhs) {
        lhs->principal(rhs);
        send<target>(std::move(lhs));
    }

    struct factory_guard {

        inline
        factory_guard() {
            ::sbn::factory.start();
        }

        inline
        ~factory_guard() {
            ::sbn::factory.stop();
            ::sbn::factory.wait();
            ::sbn::factory.clear();
        }

    };

    template<class Pipeline>
    void
    upstream(Pipeline& ppl, kernel* lhs, kernel_ptr&& rhs) {
        rhs->parent(lhs);
        ppl.send(std::move(rhs));
    }

}

#endif // vim:filetype=cpp
