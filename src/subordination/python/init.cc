#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/python/init.hh>
#include <subordination/python/python.hh>

namespace {

    void usage(std::ostream& out, char** argv) {
        out << "usage: " << argv[0] << " args...\n"
            "args    Python 3 interpreter arguemnts\n";
    }

    int nested_main(int argc, char* argv[]) {
        using namespace sbn;
        using namespace sbn::python;
        install_error_handler();
        {
            auto g = factory.types().guard();
            factory.types().add<Main>(1);
            factory.types().add<Cpp_kernel>(2);
            factory.local().num_upstream_threads(1); // Python 3 does not like multiple threads
        }
        factory_guard g;
        if (this_application::standalone()) {
            send(sbn::make_pointer<Main>(argc, argv));
        }
        return wait_and_return();
    }

}


PyMethodDef sbn::python::Py_kernel_methods[] = {
    {
        .ml_name = "__reduce__",
        .ml_meth = (PyCFunction) sbn::python::Py_kernel_reduce,
        .ml_flags = METH_NOARGS,
        .ml_doc = "Pickle support method."
    },
    {
        .ml_name = "_set_Cpp_kernel",
        .ml_meth = (PyCFunction) sbn::python::Py_kernel_set_Cpp_kernel,
        .ml_flags = METH_VARARGS,
        .ml_doc = "Set cpp kernel to python kernel."
    },
    {nullptr, nullptr, 0, nullptr}        /* Sentinel */
};

PyTypeObject sbn::python::Py_kernel_type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    "sbn.Kernel",                                       /* tp_name */
    sizeof(sbn::python::Py_kernel),                     /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor) sbn::python::Py_kernel_dealloc,        /* tp_dealloc */
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
    Py_kernel_methods,                                  /* tp_methods */
    0,                                                  /* tp_members */
    0,                                                  /* tp_getset */
    0,                                                  /* tp_base */
    0,                                                  /* tp_dict */
    0,                                                  /* tp_descr_get */
    0,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    (initproc) sbn::python::Py_kernel_init,             /* tp_init */
    0,                                                  /* tp_alloc */
    (newfunc) sbn::python::Py_kernel_new,               /* tp_new */
};

namespace {

    PyMethodDef sbn_methods[] = {
        {
            .ml_name = "upstream",
            .ml_meth = (PyCFunction) sbn::python::upstream,
            .ml_flags = METH_VARARGS | METH_KEYWORDS,
            .ml_doc = "Upstream kernel of sbn."
        },
        {
            .ml_name = "commit",
            .ml_meth = (PyCFunction) sbn::python::commit,
            .ml_flags = METH_VARARGS | METH_KEYWORDS,
            .ml_doc = "Commit kernel of sbn."
        },
        {nullptr, nullptr, 0, nullptr}        /* Sentinel */
    };

    PyModuleDef sbn_module = {
        PyModuleDef_HEAD_INIT,
        .m_name = "sbn",
        .m_doc = "Subordination framework.",
        .m_size = -1,
        .m_methods = sbn_methods
    };

    PyModuleDef sbn_target_enum = {
        PyModuleDef_HEAD_INIT,
        .m_name = "sbn.Target",
        .m_doc = "Target enum for sending kernel.",
        .m_size = -1,
        .m_methods = nullptr
    };

    PyMODINIT_FUNC sbn_init() {
        // Preparation components
        if (PyType_Ready(&sbn::python::Py_kernel_type) < 0)
            return nullptr;

        sbn::python::object t_enum = PyModule_Create(&sbn_target_enum);
        if (!t_enum) { return nullptr; }
        t_enum.retain();

        if (PyModule_AddObject(t_enum.get(), "Local", PyLong_FromLong(sbn::Local)) < 0)    // 0
            return nullptr;

        if (PyModule_AddObject(t_enum.get(), "Remote", PyLong_FromLong(sbn::Remote)) < 0)  // 1
            return nullptr;

        // Adding to sbn module
        sbn::python::object m = PyModule_Create(&sbn_module);
        if (!m) { return nullptr; }

        Py_INCREF(&sbn::python::Py_kernel_type);
        if (PyModule_AddObject(m.get(), "Kernel", (PyObject *) &sbn::python::Py_kernel_type) < 0) {
            Py_DECREF(&sbn::python::Py_kernel_type);
            return nullptr;
        }

        if (PyModule_AddObject(m.get(), "Target", t_enum.get()) < 0)
           return nullptr;

        return m.get();
    }
}

int main(int argc, char* argv[]) {
    using namespace sbn::python;
    if (argc < 2) {
        usage(std::clog, argv);
        std::exit(1);
    }
    PyImport_AppendInittab("sbn", &sbn_init);

    interpreter_guard g;
    load(argv[1]);
    set_arguments(argc, argv);

    return nested_main(argc, argv);
}
