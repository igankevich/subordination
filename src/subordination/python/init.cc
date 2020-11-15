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
            factory.types().add<kernel_map>(2);
        }
        factory_guard g;
        if (this_application::standalone()) {
            send(sbn::make_pointer<Main>(argc, argv));
        }
        return wait_and_return();
    }

}


PyMethodDef sbn::python::py_kernel_map_methods[] = {
    {
        .ml_name = "__reduce__",
        .ml_meth = (PyCFunction) sbn::python::py_kernel_map_reduce,
        .ml_flags = METH_NOARGS,
        .ml_doc = "Pickle support method."
    },
    {
        .ml_name = "_set_kernel_cpp",
        .ml_meth = (PyCFunction) sbn::python::py_kernel_map_set_kernel_cpp,
        .ml_flags = METH_VARARGS,
        .ml_doc = "Set cpp kernel."
    },
    {nullptr, nullptr, 0, nullptr}        /* Sentinel */
};

PyTypeObject sbn::python::py_kernel_map_type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
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

namespace {

    PyMethodDef sbn_methods[] = {
        {
            .ml_name = "kernel_upstream",
            .ml_meth = (PyCFunction) sbn::python::kernel_upstream,
            .ml_flags = METH_VARARGS | METH_KEYWORDS,
            .ml_doc = "Upstream kernel of sbn."
        },
        {
            .ml_name = "kernel_commit",
            .ml_meth = (PyCFunction) sbn::python::kernel_commit,
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

    PyMODINIT_FUNC sbn_init() {
        if (PyType_Ready(&sbn::python::py_kernel_map_type) < 0) {
            return nullptr;
        }
        sbn::python::object m = PyModule_Create(&sbn_module);
        if (!m) { return nullptr; }
        Py_INCREF(&sbn::python::py_kernel_map_type);
        if (PyModule_AddObject(m.get(), "kernel", (PyObject *) &sbn::python::py_kernel_map_type) < 0) {
            Py_DECREF(&sbn::python::py_kernel_map_type);
            return nullptr;
        }
        return m.get();
    }

    //std::tuple<std::string, std::string> SplitFilename(const std::string& str)
    //{
    //    std::string::size_type found = str.find_last_of("/\\");
    //    return {str.substr(0,found), str.substr(found+1)};
    //}
}

int main(int argc, char* argv[]) {
    using namespace sbn::python;
    if (argc < 2) {
        usage(std::clog, argv);
        std::exit(1);
    }
    PyImport_AppendInittab("sbn", &sbn_init);
    //pointer<wchar_t>(Py_DecodeLocale(argv[0], nullptr));
    //Py_SetProgramName(program.get());
    interpreter_guard g;
    set_arguments(argc, argv);
    //PyObject* sysPath = PySys_GetObject((char*)"path");
    //auto [_path, _name] = SplitFilename((const std::string)argv[1]);
    //PyObject* programPath = PyUnicode_FromString(_path.c_str());
    //PyList_Append(sysPath, programPath);
    //Py_DECREF(programPath);
    //argv[1] = (char*)_name.substr(0, _name.find('.')).c_str();
    return nested_main(argc, argv);
}
