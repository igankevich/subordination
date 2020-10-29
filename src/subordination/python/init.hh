#ifndef SUBORDINATION_PYTHON_INIT_HH
#define SUBORDINATION_PYTHON_INIT_HH
#include <Python.h>

#include <subordination/python/kernel.hh>

namespace sbn {
   namespace python {
      using main_function_type = void (*)(int,char**);
      void main(int argc, char* argv[], main_function_type main);

      extern PyMethodDef py_kernel_map_methods[];
      extern PyTypeObject py_kernel_map_type;
      extern PyMethodDef SbnMethods[];
      extern PyModuleDef SbnModule;
    }
}

#endif // vim:filetype=cpp
