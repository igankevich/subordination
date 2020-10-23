#ifndef SUBORDINATION_PYTHON_KERNEL_HH
#define SUBORDINATION_PYTHON_KERNEL_HH

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/core/kernel.hh>

namespace sbn {

    namespace python {

        PyObject* test_func(PyObject *self, PyObject *args);

        class Main: public sbn::kernel {

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
    }
}

#endif // vim:filetype=cpp