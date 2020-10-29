#define PY_SSIZE_T_CLEAN

#include <subordination/python/init.hh>


extern PyMethodDef sbn::python::py_kernel_map_methods[] = {
    {
        .ml_name = "test_method",
        .ml_meth = (PyCFunction) sbn::python::py_kernel_map_test_method,
        .ml_flags = METH_NOARGS,
        .ml_doc = "Test method of kernel"
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

extern PyTypeObject sbn::python::py_kernel_map_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "sbn.kernel",                                       /* tp_name */
    sizeof(sbn::python::py_kernel_map),                 /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor) sbn::python::py_kernel_map_dealloc,                 /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_reserved */
    0,                                                  /* tp_repr */
    0,                                                  /* tp_as_number */
    0,                                                  /* tp_as_sequence */
    0,                                                  /* tp_as_mapping */
    0,                                                  /* tp_hash */
    0,                                                  /* tp_call */
    0,                                                  /* tp_str */
    0,                                                  /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,           /* tp_flags */
    "Kernel class of sbn",                              /* tp_doc */
    0,                                                  /* tp_traverse */
    0,                                                  /* tp_clear */
    0,                                                  /* tp_richcompare */
    0,                                                  /* tp_weaklistoffset */
    0,                                                  /* tp_iter */
    0,                                                  /* tp_iternext */
    py_kernel_map_methods,                              /* tp_methods */
    0,                                                  /* tp_members */
    0,                                                  /* tp_getset */
    0,                                                  /* tp_base */
    0,                                                  /* tp_dict */
    0,                                                  /* tp_descr_get */
    0,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    (initproc) sbn::python::py_kernel_map_init,                      /* tp_init */
    0,                                                  /* tp_alloc */
    (newfunc) sbn::python::py_kernel_map_new,                        /* tp_new */
};  

extern PyMethodDef sbn::python::SbnMethods[] = {
{
    .ml_name = "kernel_upstream",
    .ml_meth = (PyCFunction) sbn::python::kernel_upstream,
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "Up kernel of sbn."
},
{NULL, NULL, 0, NULL}        /* Sentinel */
};

extern PyModuleDef sbn::python::SbnModule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "sbn",
    .m_doc = "Subordination framework.",
    .m_size = -1,
    .m_methods = SbnMethods
};

namespace{
    PyMODINIT_FUNC
    PyInit_sbn(void)
    {
        PyObject *m;
        if (PyType_Ready(&sbn::python::py_kernel_map_type) < 0)
            return NULL;

        m = PyModule_Create(&sbn::python::SbnModule);
        if (m == NULL)
            return NULL;

        Py_INCREF(&sbn::python::py_kernel_map_type);
        if (PyModule_AddObject(m, "kernel", (PyObject *) &sbn::python::py_kernel_map_type) < 0) {
            Py_DECREF(&sbn::python::py_kernel_map_type);
            Py_DECREF(m);
            return NULL;
        }

        return m;
    }
}

void sbn::python::main(int argc, char* argv[], main_function_type _nested_main) {

    if (argc < 2) {
        fprintf(stderr,"Usage: call pythonfile [args]\n");
        exit(1);
    }

    PyImport_AppendInittab("sbn", &PyInit_sbn);

    Py_Initialize();

    PyObject *sys_path = PySys_GetObject("path");
    PyList_Append(sys_path, PyUnicode_FromString("."));

    _nested_main(argc, argv);

    if (Py_FinalizeEx() < 0)
        exit(120);
}
