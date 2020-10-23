#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>

namespace {

    static PyMethodDef SbnMethods[] = {
        {
            .ml_name = "test_func",
            .ml_meth = sbn::python::test_func,
            .ml_flags = METH_VARARGS,
            .ml_doc = "Execute a shell command."
        },
        {NULL, NULL, 0, NULL}        /* Sentinel */
    };

    static PyModuleDef SbnModule = {
        PyModuleDef_HEAD_INIT,
        .m_name = "sbn",
        .m_doc = "Subordination framework.",
        .m_size = -1,
        .m_methods = SbnMethods
    };

    PyMODINIT_FUNC
    PyInit_sbn(void)
    {
        return PyModule_Create(&SbnModule);
    }
}

void sbn::python::main(int argc, char* argv[], main_function_type _nested_main) {

    if (argc < 3) {
        fprintf(stderr,"Usage: call pythonfile funcname [args]\n");
        exit(1);
    }

    PyImport_AppendInittab("sbn", &PyInit_sbn);

    _nested_main(argc, argv);
}
