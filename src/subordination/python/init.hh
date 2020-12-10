#ifndef SUBORDINATION_PYTHON_INIT_HH
#define SUBORDINATION_PYTHON_INIT_HH

#include <Python.h>
#include <memory>

#include <subordination/python/kernel.hh>

namespace sbn {
    namespace python {

        extern PyMethodDef Py_kernel_methods[];
        extern PyTypeObject Py_kernel_type;

    }
}

#endif // vim:filetype=cpp
