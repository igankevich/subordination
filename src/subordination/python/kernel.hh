#ifndef SUBORDINATION_PYTHON_KERNEL_HH
#define SUBORDINATION_PYTHON_KERNEL_HH

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/core/kernel.hh>

namespace sbn {

    namespace python {

        PyObject* test_func(PyObject *self, PyObject *args);

        // ================ kernel_map ================

        typedef struct {
            PyObject_HEAD
            PyObject *_act;
            PyObject *_react;
        } kernel_map;

        void kernel_map_dealloc(kernel_map *self);
        PyObject * kernel_map_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
        int kernel_map_init(kernel_map *self, PyObject *args, PyObject *kwds);

        PyObject* kernel_map_test_method(kernel_map *self, PyObject *Py_UNUSED(ignored));

        // ============================================

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