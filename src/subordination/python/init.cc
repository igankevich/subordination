#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/python/init.hh>

namespace {

    sbn::python::main_function_type nested_main;
}

void sbn::python::main(int argc, char* argv[], main_function_type _nested_main) {
    ::nested_main = _nested_main;

    PyObject *pName, *pModule, *pFunc;
    PyObject *pValue;

    if (argc < 3) {
        fprintf(stderr,"Usage: call pythonfile funcname [args]\n");
        exit(1);
    }

    Py_Initialize();

    PyObject *sys_path = PySys_GetObject("path");
    PyList_Append(sys_path, PyUnicode_FromString("."));

    pName = PyUnicode_DecodeFSDefault(argv[1]);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, argv[2]);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {           
            pValue = PyObject_CallObject(pFunc, NULL);
            if (pValue == NULL) {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                exit(1);
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", argv[2]);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", argv[1]);
        exit(1);
    }
    if (Py_FinalizeEx() < 0) {
        exit(120);
    }

    nested_main(argc, argv);
}
