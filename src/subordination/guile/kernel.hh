#ifndef SUBORDINATION_GUILE_KERNEL_HH
#define SUBORDINATION_GUILE_KERNEL_HH

#include <libguile.h>

#include <subordination/core/kernel.hh>
#include <subordination/guile/base.hh>

namespace sbn {

    namespace guile {

        class Kernel: public sbn::kernel {

        protected:
            SCM _act = SCM_UNSPECIFIED;
            SCM _react = SCM_UNSPECIFIED;
            SCM _data{scm_make_undefined_variable()};

        public:

            Kernel() = default;
            inline Kernel(SCM act, SCM react, SCM data) noexcept:
            _act(act), _react(react), _data(scm_make_variable(data)) {}

            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;

            inline Kernel& operator=(const Kernel& rhs) {
                this->_act = rhs._act;
                this->_react = rhs._react;
                this->_data = rhs._data;
                return *this;
            }

            inline SCM data() noexcept { return this->_data; }

        };

        void kernel_define();

        class Main: public Kernel {

        public:

            Main() = default;

            inline Main(int argc, char** argv) {
                if (argc >= 2) { load_main_kernel(argv[1]); }
            }

            inline Main& operator=(const Kernel& rhs) {
                this->Kernel::operator=(rhs);
                return *this;
            }

            void read(sbn::kernel_buffer& in) override;

        private:

            void load_main_kernel(const char* path);

        };

    }
}

#endif // vim:filetype=cpp
