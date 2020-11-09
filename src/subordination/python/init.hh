#ifndef SUBORDINATION_PYTHON_INIT_HH
#define SUBORDINATION_PYTHON_INIT_HH

#include <Python.h>

#include <memory>

#include <subordination/python/kernel.hh>

namespace sbn {
    namespace python {

        extern PyMethodDef py_kernel_map_methods[];
        extern PyTypeObject py_kernel_map_type;

    }
}

#endif // vim:filetype=cpp
