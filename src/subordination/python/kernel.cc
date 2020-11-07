#include <sstream>
#include <typeinfo>

#include <subordination/api.hh>
#include <subordination/python/kernel.hh>
#include <subordination/python/init.hh>


PyObject* sbn::python::kernel_upstream(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"parent", "child", NULL};
    PyObject *_py_kernel_parent = NULL, *_py_kernel_child = NULL;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!O!", kwlist,
        &sbn::python::py_kernel_map_type, 
        &_py_kernel_parent,
        &sbn::python::py_kernel_map_type,
        &_py_kernel_child
        ))
    {
        return NULL;
    }
    Py_INCREF(_py_kernel_parent);
    Py_INCREF(_py_kernel_child);

    sbn::python::py_kernel_map* _kernel_parent = (sbn::python::py_kernel_map*)_py_kernel_parent;
    sbn::python::py_kernel_map* _kernel_child = (sbn::python::py_kernel_map*)_py_kernel_child;

    sbn::upstream<sbn::Remote>(std::move(_kernel_parent->_kernel_map), std::unique_ptr<sbn::python::kernel_map>(std::move(_kernel_child->_kernel_map)));

    Py_RETURN_NONE;
}

PyObject* sbn::python::kernel_commit(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"kernel", NULL};
    PyObject *_py_kernel = NULL;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!", kwlist,
        &sbn::python::py_kernel_map_type, 
        &_py_kernel
        ))
    {
        return NULL;
    }
    Py_INCREF(_py_kernel);

    sbn::python::py_kernel_map* _kernel = (sbn::python::py_kernel_map*)_py_kernel;

    sbn::commit<sbn::Remote>(std::unique_ptr<sbn::python::kernel_map>(std::move(_kernel->_kernel_map)));

    Py_RETURN_NONE;
}


void sbn::python::py_kernel_map_dealloc(sbn::python::py_kernel_map* self)
{
    /* Custom deallocation behavior */

    // ...

    // Default deallocation behavior
    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* sbn::python::py_kernel_map_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    /* Custom allocation behavior */

    // Default allocation behavior
    sbn::python::py_kernel_map *self;
    self = (sbn::python::py_kernel_map *) type->tp_alloc(type, 0);

    // Init _kernel_map
    if (sbn::python::_main != nullptr)
    {
        self->_kernel_map = std::move(sbn::python::_main);
        sbn::python::_main = nullptr;
    }
    else
    {
        self->_kernel_map = new sbn::python::kernel_map(std::move((PyObject*)self));
    }


    return (PyObject *) self;
}

int sbn::python::py_kernel_map_init(sbn::python::py_kernel_map* self, PyObject* args, PyObject* kwds)
{
    /* Initialization of kernel */

    return 0;
}


PyObject* sbn::python::py_kernel_map_test_method(py_kernel_map* self, PyObject* Py_UNUSED(ignored))
{
    // PyObject *func_args = NULL;
    // // func_args = Py_BuildValue("(ii)", 5, 10);

    // PyObject *func_ret = NULL;
    // func_ret = PyEval_CallObject(self->_act, func_args);

    // char *ret = NULL;
    // PyArg_Parse(func_ret, "s", &ret);

    // std::stringstream msg;
    // // msg << "Result of kernel._act method: " << ret << '\n';
    // msg << "Work!";
    // std::clog << msg.str() << std::flush;

    Py_RETURN_NONE;
}


void sbn::python::kernel_map::act() {
    sys::log_message(">>>> Sbn", "Kernel_map.act");
    PyObject_CallMethod(this->py_k_map(), "act", NULL);
}

void sbn::python::kernel_map::react(sbn::kernel_ptr&& child_ptr){
    sys::log_message(">>>> Sbn", "Kernel_map.react");
    auto child = sbn::pointer_dynamic_cast<sbn::python::kernel_map>(std::move(child_ptr));
    PyObject_CallMethod(this->py_k_map(), "react", "O", child->py_k_map());
}


void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");

    if (auto* a = target_application()) {
        const auto& args = a->arguments();    

        PyObject *pName, *pModule, *pMainClass;;
        PyObject *pValue;

        // PyGILState_STATE gstate;
        // gstate = PyGILState_Ensure();

        pName = PyUnicode_DecodeFSDefault(args[0].c_str());
        /* Error checking of pName left out */

        pModule = PyImport_Import(pName);
        Py_DECREF(pName);

        if (pModule != NULL) {

            sbn::python::_main = this;
            pMainClass = PyObject_CallMethod(pModule, "Main", NULL);
            this->py_k_map(std::move(pMainClass));

            pValue = PyObject_CallMethod(pMainClass, "act", NULL);
            if (pValue == NULL) {
                Py_DECREF(pMainClass);
                Py_DECREF(pModule);

                PyErr_Print();
                sbn::exit(1);
            }
                    
            Py_XDECREF(pMainClass);
            Py_DECREF(pModule);
        }
        else {
            PyErr_Print();
            sbn::exit(1);
        }

        // PyGILState_Release(gstate);
    }

    // auto* ppl = source_pipeline() ? source_pipeline() : &sbn::factory.local();
    // if (!parent()) {
    //     sbn::exit(0);
    // } else {
    //     return_to_parent();
    //     ppl->send(std::move(this_ptr()));
    // }
}


sbn::python::Main* sbn::python::_main = nullptr;
