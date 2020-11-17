#ifndef SUBORDINATION_PYTHON_KERNEL_HH
#define SUBORDINATION_PYTHON_KERNEL_HH

#include <Python.h>

#include <subordination/core/kernel.hh>

namespace sbn {

    namespace python {

        class kernel_map;

        // ================ kernel_map ================

        struct py_kernel_map {
            PyObject_HEAD
            // kernel_map* _kernel_map;
            PyObject* _kernel_map_capsule;
        };

        void py_kernel_map_dealloc(py_kernel_map* self);
        PyObject* py_kernel_map_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
        int py_kernel_map_init(py_kernel_map* self, PyObject* args, PyObject* kwds);

        PyObject* py_kernel_map_reduce(py_kernel_map *self, PyObject *Py_UNUSED(ignored));
        PyObject* py_kernel_map_set_kernel_cpp(py_kernel_map* self, PyObject * args);

        // ============================================

        class kernel_map: public sbn::kernel {

        private:
            PyObject* _py_k_map;
        public:

            kernel_map() = default;
            inline kernel_map(PyObject* py_k_map) noexcept: _py_k_map(py_k_map){Py_INCREF(this->_py_k_map);}

            ~kernel_map() noexcept{Py_DECREF(this->_py_k_map);}

            PyObject* py_k_map(){return this->_py_k_map;} // Getter
            void py_k_map(PyObject* py_k_map){this->_py_k_map = py_k_map;} // Setter

            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void write(sbn::kernel_buffer& out) const override;
            void read(sbn::kernel_buffer& in) override; 

        };

        class Main: public kernel_map {

        public:

            Main() = default;

            inline Main(int argc, char** argv) {
                application::string_array args;
                args.reserve(argc);
                for (int i=0; i<argc; ++i) { args.emplace_back(argv[i]); }
                if (!target_application()) {
                    std::unique_ptr<sbn::application> app{new sbn::application(args, {})};
                    target_application(app.release());
                }
            }

            void act() override;
            void read(sbn::kernel_buffer& in) override;

        };

        PyObject* kernel_upstream(PyObject *self, PyObject *args, PyObject *kwds);
        PyObject* kernel_commit(PyObject *self, PyObject *args, PyObject *kwds);

    }
}

#endif // vim:filetype=cpp
