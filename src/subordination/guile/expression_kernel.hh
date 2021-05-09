#ifndef SUBORDINATION_GUILE_EXPRESSION_KERNEL_HH
#define SUBORDINATION_GUILE_EXPRESSION_KERNEL_HH

#include <libguile.h>

#include <subordination/core/kernel.hh>
#include <subordination/guile/base.hh>
#include <subordination/api.hh>
#include <subordination/guile/base.hh>

#include <vector>
#include <map>

namespace sbn {

    namespace guile {

        class expression_kernel_main : public sbn::kernel {
            protected_scm _port = SCM_UNSPECIFIED;
            protected_scm _scheme = SCM_UNSPECIFIED;
            std::map<std::string, protected_scm> _definitions;
        public:
            expression_kernel_main() {}
            expression_kernel_main(int argc, char* argv[]);
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };

        class expression_kernel :  public sbn::kernel {
            int _parent_arg;
        protected:
            protected_scm _environment;
            std::vector<protected_scm> _args;
            size_t _finished_child = 0;
            const protected_scm _scheme = SCM_UNSPECIFIED;
            protected_scm _result = SCM_UNSPECIFIED;
            std::map<std::string, protected_scm> _definitions;
            
        public:
            expression_kernel() {}
            expression_kernel(protected_scm scheme, std::map<std::string, protected_scm> const & def, int arg = 0)
                : _parent_arg(arg)
                , _environment(scm_interaction_environment())
                , _scheme(scheme)
                , _definitions(def) {}
            expression_kernel(protected_scm scheme, protected_scm env, std::map<std::string, protected_scm> const & def, int arg = 0)
                : _parent_arg(arg)
                , _environment(env)
                , _scheme(scheme)
                , _definitions(def) {}
            virtual void act() override;
            virtual void react(sbn::kernel_ptr&& child) override;
            virtual void read(sbn::kernel_buffer& in) override;
            virtual void write(sbn::kernel_buffer& out) const override;
            SCM get_result() const {return _result.get();}
            int get_arg() const {return _parent_arg;}
            void set_environment(SCM env) {_environment = env; }

            std::map<std::string, protected_scm> & get_defs() { return this->_definitions; }
            std::map<std::string, protected_scm> const & get_defs() const { return this->_definitions; }
        };

        class expression_kernel_if : public expression_kernel {
        
        public:
            expression_kernel_if() {}
            expression_kernel_if(protected_scm scm, std::map<std::string, protected_scm> const & def): expression_kernel(scm, def, -2) {}
            expression_kernel_if(protected_scm scm, protected_scm env, std::map<std::string, protected_scm> const & def): expression_kernel(scm, env, def, -2) {}
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };

        class expression_kernel_define : public expression_kernel {
            protected_scm _head = SCM_UNSPECIFIED;
            protected_scm _body = SCM_UNSPECIFIED;
            protected_scm _body_args = SCM_UNSPECIFIED;
        public:
            expression_kernel_define() {}            
            expression_kernel_define(protected_scm scm, std::map<std::string, protected_scm> const & def, protected_scm args = SCM_UNSPECIFIED, int arg = 0);
            expression_kernel_define(protected_scm scm, protected_scm env,  std::map<std::string, protected_scm> const & def, protected_scm args = SCM_UNSPECIFIED, int arg = 0);
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };

        class expression_kernel_let : public expression_kernel {
            protected_scm _seq = SCM_UNSPECIFIED;
        public:
            expression_kernel_let() {}
            expression_kernel_let(protected_scm scm, std::map<std::string, protected_scm> const & def): expression_kernel(scm, def, -2) {}
            expression_kernel_let(protected_scm scm, protected_scm env, std::map<std::string, protected_scm> const & def): expression_kernel(scm, env, def, -2) {}
            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };
    }
}

#endif