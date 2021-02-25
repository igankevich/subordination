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
            ~Kernel() noexcept;

            inline Kernel(SCM act, SCM react, SCM data) noexcept:
            _act(act), _react(react), _data(scm_make_variable(data)) {
                scm_gc_protect_object(this->_act);
                scm_gc_protect_object(this->_react);
                scm_gc_protect_object(this->_data);
            }

            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;

            inline Kernel& operator=(Kernel&& rhs) {
                this->_act = rhs._act;
                this->_react = rhs._react;
                this->_data = rhs._data;
                rhs._act = SCM_UNSPECIFIED;
                rhs._react = SCM_UNSPECIFIED;
                rhs._data = SCM_UNSPECIFIED;
                return *this;
            }

            inline SCM data() noexcept { return this->_data; }

        };

        void kernel_define();

        class Kernel_base: public sbn::kernel {

        private:
            std::vector<std::pair<sbn::kernel_ptr,pipeline*>> _children;
            sys::u32 _num_children{0};
            protected_scm _result = SCM_UNSPECIFIED;
            protected_scm _react = SCM_UNSPECIFIED;
            protected_scm _postamble = SCM_UNSPECIFIED;
            bool _no_children = false;

        public:
            void react(sbn::kernel_ptr&& child) override;
            void upstream(sbn::kernel_ptr&& child, pipeline* ppl);
            inline int num_children() const noexcept { return this->_num_children; }
            inline void result(SCM rhs) noexcept { this->_result = rhs; }
            inline SCM result() noexcept { return this->_result; }
            inline void react(SCM rhs) noexcept { this->_react = rhs; }
            inline void postamble(SCM rhs) noexcept { this->_postamble = rhs; }
            inline bool no_children() const noexcept { return this->_no_children; }
            void upstream_children();
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;
        };

        class Main: public Kernel_base {

        public:

            Main() = default;

            inline Main(int argc, char** argv) {
                application::string_array args;
                args.reserve(argc);
                for (int i=1; i<argc; ++i) { args.emplace_back(argv[i]); }
                if (!target_application()) {
                    std::unique_ptr<sbn::application> app{new sbn::application(args, {})};
                    target_application(app.release());
                }
            }

            void act() override;

        };

        class Map_kernel: public Kernel_base {

        private:
            protected_scm _proc = SCM_UNSPECIFIED;
            protected_scm _lists = SCM_UNSPECIFIED;
            protected_scm _pipeline = SCM_UNSPECIFIED;
            protected_scm _block_size = SCM_UNDEFINED;
            int _num_kernels = 0;

        public:
            Map_kernel() = default;

            inline Map_kernel(SCM proc, SCM lists, SCM pipeline, SCM block_size) noexcept:
            _proc(proc), _lists(lists), _pipeline(pipeline), _block_size(block_size) {}

            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;

        };

        class Map_child_kernel: public Kernel_base {

        private:
            protected_scm _proc = SCM_UNSPECIFIED;
            protected_scm _lists = SCM_UNSPECIFIED;
            protected_scm _pipeline = SCM_UNSPECIFIED;

        public:
            Map_child_kernel() = default;

            inline Map_child_kernel(SCM proc, SCM lists, SCM pipeline) noexcept:
            _proc(proc), _lists(lists), _pipeline(pipeline) {}

            void act() override;
            void read(sbn::kernel_buffer& in) override;
            void write(sbn::kernel_buffer& out) const override;

        };

    }
}

#endif // vim:filetype=cpp
