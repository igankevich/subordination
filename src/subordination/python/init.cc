#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>

namespace {

    static PyMethodDef kernel_map_methods[] = {
        {
            .ml_name = "test_method",
            .ml_meth = (PyCFunction) sbn::python::kernel_map_test_method,
            .ml_flags = METH_NOARGS,
            .ml_doc = "Test method of kernel"
        },
        {NULL, NULL, 0, NULL}        /* Sentinel */
    };

    static PyTypeObject kernel_map_type = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "sbn.kernel",                                   /* tp_name */
        sizeof(sbn::python::kernel_map),                /* tp_basicsize */
        0,                                              /* tp_itemsize */
        (destructor) sbn::python::kernel_map_dealloc,   /* tp_dealloc */
        0,                                              /* tp_print */
        0,                                              /* tp_getattr */
        0,                                              /* tp_setattr */
        0,                                              /* tp_reserved */
        0,                                              /* tp_repr */
        0,                                              /* tp_as_number */
        0,                                              /* tp_as_sequence */
        0,                                              /* tp_as_mapping */
        0,                                              /* tp_hash */
        0,                                              /* tp_call */
        0,                                              /* tp_str */
        0,                                              /* tp_getattro */
        0,                                              /* tp_setattro */
        0,                                              /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,       /* tp_flags */
        "Kernel class of sbn",                          /* tp_doc */
        0,                                              /* tp_traverse */
        0,                                              /* tp_clear */
        0,                                              /* tp_richcompare */
        0,                                              /* tp_weaklistoffset */
        0,                                              /* tp_iter */
        0,                                              /* tp_iternext */
        kernel_map_methods,                             /* tp_methods */
        0,                                              /* tp_members */
        0,                                              /* tp_getset */
        0,                                              /* tp_base */
        0,                                              /* tp_dict */
        0,                                              /* tp_descr_get */
        0,                                              /* tp_descr_set */
        0,                                              /* tp_dictoffset */
        (initproc) sbn::python::kernel_map_init,        /* tp_init */
        0,                                              /* tp_alloc */
        (newfunc) sbn::python::kernel_map_new,          /* tp_new */
    };

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
        PyObject *m;
        if (PyType_Ready(&kernel_map_type) < 0)
            return NULL;

        m = PyModule_Create(&SbnModule);
        if (m == NULL)
            return NULL;

        Py_INCREF(&kernel_map_type);
        if (PyModule_AddObject(m, "kernel", (PyObject *) &kernel_map_type) < 0) {
            Py_DECREF(&kernel_map_type);
            Py_DECREF(m);
            return NULL;
        }

        return m;
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
