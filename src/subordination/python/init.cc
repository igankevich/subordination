#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/python/init.hh>

namespace {

    sbn::python::main_function_type nested_main;
}

void sbn::python::main(int argc, char* argv[], main_function_type _nested_main) {
    ::nested_main = _nested_main;

    wchar_t *program = Py_DecodeLocale(argv[0], NULL);
    if (program == NULL) {
        fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
        exit(1);
    }
    Py_SetProgramName(program);  /* optional but recommended */
    Py_Initialize();
    PyRun_SimpleString("from time import time,ctime\n"
                    "print('Today is', ctime(time()))\n");
    if (Py_FinalizeEx() < 0) {
        exit(120);
    }
    PyMem_RawFree(program);
    
    nested_main(argc, argv);
}
