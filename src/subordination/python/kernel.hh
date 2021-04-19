#ifndef SUBORDINATION_PYTHON_KERNEL_HH
#define SUBORDINATION_PYTHON_KERNEL_HH

#include <subordination/core/kernel.hh>
#include <subordination/python/python.hh>

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

        PyObject* Py_kernel_enable_carries_parent(Py_kernel *self, PyObject *Py_UNUSED(ignored));

        // ============================================

        class Cpp_kernel: public sbn::kernel {

        protected:
            object _py_kernel_obj;

        public:

            Cpp_kernel() = default;
            inline Cpp_kernel(PyObject* py_kernel_obj) noexcept:
            _py_kernel_obj(py_kernel_obj) {
                // TODO Calling retain() is a workaround. We have to replace all bare
                // pointers with object wrapper.
                // Incorrect reference counting causes keyboard interrupts in benchmarks.
                this->_py_kernel_obj.retain();
            }

            inline const PyObject* py_kernel_obj() const noexcept { return this->_py_kernel_obj.get(); }
            inline PyObject* py_kernel_obj() noexcept { return this->_py_kernel_obj.get(); }
            inline void py_kernel_obj(object&& rhs) noexcept { this->_py_kernel_obj = std::move(rhs); }

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

        PyObject* mutex_lock(Py_kernel* self, PyObject* Py_UNUSED(ignored));
        PyObject* mutex_unlock(Py_kernel* self, PyObject* Py_UNUSED(ignored));
        PyObject* sleep(Py_kernel* self, PyObject *args);
    }
}

#endif // vim:filetype=cpp
