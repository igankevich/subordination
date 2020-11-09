#include <sstream>
#include <typeinfo>

#include <subordination/api.hh>
#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>
#include <subordination/python/python.hh>

namespace {
    constexpr const char* upstream_kwlist[] = {"parent", "child", nullptr};
    constexpr const char* commit_kwlist[] = {"kernel", nullptr};
    sbn::python::Main* main_kernel{};
}

PyObject* sbn::python::kernel_upstream(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *_py_kernel_parent = nullptr, *_py_kernel_child = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!O!", const_cast<char**>(upstream_kwlist),
        &py_kernel_map_type,
        &_py_kernel_parent,
        &py_kernel_map_type,
        &_py_kernel_child
        ))
    {
        return nullptr;
    }
    Py_INCREF(_py_kernel_parent);
    Py_INCREF(_py_kernel_child);

    auto _kernel_parent = (py_kernel_map*)_py_kernel_parent;
    auto _kernel_child = (py_kernel_map*)_py_kernel_child;

    sbn::upstream<sbn::Remote>(std::move(_kernel_parent->_kernel_map),
                               std::unique_ptr<kernel_map>(std::move(_kernel_child->_kernel_map)));

    Py_RETURN_NONE;
}

PyObject* sbn::python::kernel_commit(PyObject *self, PyObject *args, PyObject *kwds) {
    PyObject *_py_kernel = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!", const_cast<char**>(commit_kwlist),
        &py_kernel_map_type,
        &_py_kernel
        ))
    {
        return nullptr;
    }
    Py_INCREF(_py_kernel);

    auto _kernel = (py_kernel_map*)_py_kernel;

    sbn::commit<sbn::Remote>(std::unique_ptr<kernel_map>(std::move(_kernel->_kernel_map)));

    Py_RETURN_NONE;
}


void sbn::python::py_kernel_map_dealloc(py_kernel_map* self)
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
    auto self = (py_kernel_map *) type->tp_alloc(type, 0);

    // Init _kernel_map
    if (main_kernel != nullptr) {
        self->_kernel_map = std::move(main_kernel);
        main_kernel = nullptr;
    }
    else
    {
        self->_kernel_map = new kernel_map(std::move((PyObject*)self));
    }


    return (PyObject *) self;
}

int sbn::python::py_kernel_map_init(py_kernel_map* self, PyObject* args, PyObject* kwds)
{
    /* Initialization of kernel */

    return 0;
}


PyObject* sbn::python::py_kernel_map_test_method(py_kernel_map* self, PyObject* Py_UNUSED(ignored))
{
    // PyObject *func_args = nullptr;
    // // func_args = Py_BuildValue("(ii)", 5, 10);

    // PyObject *func_ret = nullptr;
    // func_ret = PyEval_CallObject(self->_act, func_args);

    // char *ret = nullptr;
    // PyArg_Parse(func_ret, "s", &ret);

    // std::stringstream msg;
    // // msg << "Result of kernel._act method: " << ret << '\n';
    // msg << "Work!";
    // std::clog << msg.str() << std::flush;

    Py_RETURN_NONE;
}


void sbn::python::kernel_map::act() {
    sys::log_message(">>>> Sbn", "Kernel_map.act");
    PyObject_CallMethod(this->py_k_map(), "act", nullptr);
}

void sbn::python::kernel_map::react(sbn::kernel_ptr&& child_ptr){
    sys::log_message(">>>> Sbn", "Kernel_map.react");
    auto child = sbn::pointer_dynamic_cast<kernel_map>(std::move(child_ptr));
    PyObject_CallMethod(this->py_k_map(), "react", "O", child->py_k_map());
}


void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");
    if (auto* a = target_application()) {
        const auto& args = a->arguments();
        // PyGILState_STATE gstate;
        // gstate = PyGILState_Ensure();
        //pName = PyUnicode_DecodeFSDefault(args[0].c_str());
        load(args[0].data());
        object main_module = PyImport_Import(object(PyUnicode_DecodeFSDefault("__main__")).get());
        if (main_module) {
            main_kernel = this;
            object main_class = PyObject_CallMethod(main_module, "Main", nullptr);
            this->py_k_map(std::move(main_class));
            object pValue = PyObject_CallMethod(main_class, "act", nullptr);
            if (!pValue) {
                PyErr_Print();
                sbn::exit(1);
            }
        } else {
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
