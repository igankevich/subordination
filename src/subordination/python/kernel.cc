#include <sstream>

#include <subordination/api.hh>
#include <subordination/python/kernel.hh>

PyObject* sbn::python::test_func(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
    sts = system(command);
    return PyLong_FromLong(sts);
}

void sbn::python::kernel_map_dealloc(sbn::python::kernel_map *self)
{
    /* Custom deallocation behavior */

    // ...

    // Default deallocation behavior
    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* sbn::python::kernel_map_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    /* Custom allocation behavior */

    // Default allocation behavior
    sbn::python::kernel_map *self;
    self = (sbn::python::kernel_map *) type->tp_alloc(type, 0);

    // ...
    // self->kernel = sbn::make_pointer<sbn::python::Kernel>();

    return (PyObject *) self;
}

int sbn::python::kernel_map_init(sbn::python::kernel_map *self, PyObject *args, PyObject *kwds)
{
    /* Initialization of kernel */

    static char *kwlist[] = {"act", "react", NULL};
    PyObject *act = NULL, *react = NULL; 
    // PyObject *tmp = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OO", kwlist, &act, &react))
        return -1;

    if (act) {
        // tmp = self->_act;
        Py_INCREF(act);
        self->_act = act;
        // Py_DECREF(tmp);
    }
    if (react) {
        // tmp = self->_react;
        Py_INCREF(react);
        self->_react = react;
        // Py_DECREF(tmp);
    }

    return 0;
}


PyObject* sbn::python::kernel_map_test_method(kernel_map *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *func_args = NULL;
    // func_args = Py_BuildValue("(ii)", 5, 10);

    PyObject *func_ret = NULL;
    func_ret = PyEval_CallObject(self->_act, func_args);

    char *ret = NULL;
    PyArg_Parse(func_ret, "s", &ret);

    std::stringstream msg;
    msg << "Result of kernel._act method: " << ret << '\n';
    std::clog << msg.str() << std::flush;

    Py_RETURN_NONE;
}


void sbn::python::Main::act() {
    if (auto* a = target_application()) {
        const auto& args = a->arguments();    

        PyObject *pName, *pModule, *pFunc;
        PyObject *pValue;

        Py_Initialize();

        PyObject *sys_path = PySys_GetObject("path");
        PyList_Append(sys_path, PyUnicode_FromString("."));

        pName = PyUnicode_DecodeFSDefault(args[0].c_str());
        /* Error checking of pName left out */

        pModule = PyImport_Import(pName);
        Py_DECREF(pName);

        if (pModule != NULL) {
            pFunc = PyObject_GetAttrString(pModule, args[1].c_str());
            /* pFunc is a new reference */

            if (pFunc && PyCallable_Check(pFunc)) {           
                pValue = PyObject_CallObject(pFunc, NULL);
                if (pValue == NULL) {
                    Py_DECREF(pFunc);
                    Py_DECREF(pModule);
                    
                }
            }
            else if (PyErr_Occurred()){
                PyErr_Print();
                sbn::exit(1);
            }
                    
            Py_XDECREF(pFunc);
            Py_DECREF(pModule);
        }
        else {
            PyErr_Print();
            sbn::exit(1);
        }
        if (Py_FinalizeEx() < 0)
            sbn::exit(120);
    }

    // auto* ppl = source_pipeline() ? source_pipeline() : &sbn::factory.local();
    // if (!parent()) {
    //     sbn::exit(0);
    // } else {
    //     return_to_parent();
    //     ppl->send(std::move(this_ptr()));
    // }
}