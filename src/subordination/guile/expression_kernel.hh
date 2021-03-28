#ifndef SUBORDINATION_GUILE_EXPRESSION_KERNEL_HH
#define SUBORDINATION_GUILE_EXPRESSION_KERNEL_HH

#include <libguile.h>

#include <subordination/core/kernel.hh>
#include <subordination/guile/base.hh>
#include <subordination/api.hh>
#include <subordination/guile/base.hh>

#include <vector>

namespace sbn {

    namespace guile {

        class expression_kernel_main : public sbn::kernel {
            protected_scm _scheme = SCM_UNSPECIFIED;
        public:
            expression_kernel_main() {}
            expression_kernel_main(int argc, char* argv[]);
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };

        class expression_kernel :  public sbn::kernel {
            SCM _scheme = SCM_UNSPECIFIED;
            protected_scm _result = SCM_UNSPECIFIED;
            std::vector<protected_scm> _args;
            int _parent_arg;
            size_t _finished_child = 0;
            
        public:
            expression_kernel() {}
            expression_kernel(SCM scheme, int arg = 0): _scheme(scheme), _parent_arg(arg) {}
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
            SCM get_result() const {return _result.get();}
            int get_arg() const {return _parent_arg;}
        };
    }
}

#endif