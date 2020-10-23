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