#ifndef SUBORDINATION_PYTHON_KERNEL_HH
#define SUBORDINATION_PYTHON_KERNEL_HH

#include <Python.h>

#include <subordination/core/kernel.hh>

namespace sbn {

    namespace python {

        class Cpp_kernel;

        // ================ Cpp_kernel ================

        struct Py_kernel {
            PyObject_HEAD
            PyObject* _cpp_kernel_capsule;
        };

        void Py_kernel_dealloc(Py_kernel* self);
        PyObject* Py_kernel_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
        int Py_kernel_init(Py_kernel* self, PyObject* args, PyObject* kwds);

        PyObject* Py_kernel_reduce(Py_kernel *self, PyObject *Py_UNUSED(ignored));
        PyObject* Py_kernel_set_Cpp_kernel(Py_kernel* self, PyObject * args);

        // ============================================

        class Cpp_kernel: public sbn::kernel {

        private:
            PyObject* _py_kernel_obj;
        public:

            Cpp_kernel() = default;
            inline Cpp_kernel(PyObject* py_kernel_obj) noexcept: _py_kernel_obj(py_kernel_obj){Py_INCREF(this->_py_kernel_obj);}

            ~Cpp_kernel() noexcept{Py_DECREF(this->_py_kernel_obj);}

            PyObject* py_kernel_obj(){return this->_py_kernel_obj;} // Getter
            void py_kernel_obj(PyObject* py_kernel_obj){this->_py_kernel_obj = py_kernel_obj;} // Setter

            void act() override;
            void react(sbn::kernel_ptr&& child) override;
            void write(sbn::kernel_buffer& out) const override;
            void read(sbn::kernel_buffer& in) override; 

        };

        class Main: public Cpp_kernel {

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

        PyObject* upstream(PyObject *self, PyObject *args, PyObject *kwds);
        PyObject* commit(PyObject *self, PyObject *args, PyObject *kwds);

    }
}

#endif // vim:filetype=cpp
