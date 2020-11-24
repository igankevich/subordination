#include <sstream>
#include <typeinfo>

#include <subordination/api.hh>
#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>
#include <subordination/python/python.hh>

namespace {
    constexpr const char* upstream_kwlist[] = {"parent", "child", nullptr};
    constexpr const char* commit_kwlist[] = {"kernel", nullptr};
}

PyObject* sbn::python::kernel_upstream(PyObject *self, PyObject *args, PyObject *kwds){
    sys::log_message(">>>> Sbn", "upstream");
    PyObject *_py_kernel_parent = nullptr, *_py_kernel_child = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!O!", const_cast<char**>(upstream_kwlist),
        &py_kernel_map_type,
        &_py_kernel_parent,
        &py_kernel_map_type,
        &_py_kernel_child))
        return nullptr;

    Py_INCREF(_py_kernel_parent);
    Py_INCREF(_py_kernel_child);

    auto _kernel_parent = (py_kernel_map*)_py_kernel_parent;
    auto _kernel_child = (py_kernel_map*)_py_kernel_child;

    auto _kernel_parent_ptr = (sbn::python::kernel_map*)PyCapsule_GetPointer(_kernel_parent->_kernel_map_capsule, "ptr");
    auto _kernel_child_ptr = (sbn::python::kernel_map*)PyCapsule_GetPointer(_kernel_child->_kernel_map_capsule, "ptr");

    sbn::upstream<sbn::Remote>(std::move(_kernel_parent_ptr),
                               std::unique_ptr<kernel_map>(std::move(_kernel_child_ptr)));

    Py_RETURN_NONE;
}

PyObject* sbn::python::kernel_commit(PyObject *self, PyObject *args, PyObject *kwds) {
    sys::log_message(">>>> Sbn", "commit");
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

    auto _kernel_ptr = (sbn::python::kernel_map*)PyCapsule_GetPointer(_kernel->_kernel_map_capsule, "ptr");

    sbn::commit<sbn::Remote>(std::unique_ptr<kernel_map>(std::move(_kernel_ptr)));
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

    return (PyObject *) self;
}

int sbn::python::py_kernel_map_init(py_kernel_map* self, PyObject* args, PyObject* kwds)
{
    /* Initialization of kernel */
    PyObject* capsule_kernel_cpp;
    PyArg_UnpackTuple(args, "ptr", 0, 1, &capsule_kernel_cpp);

    if (PyCapsule_IsValid(capsule_kernel_cpp, "ptr"))
    {
        sys::log_message("test", "py_kernel_main.__init__");
        Py_INCREF(capsule_kernel_cpp);
        self->_kernel_map_capsule = capsule_kernel_cpp;
    }
    else
    {
        auto _kernel_map = new kernel_map(std::move((PyObject*)self));
        self->_kernel_map_capsule = PyCapsule_New((void *)_kernel_map, "ptr", nullptr);
    }

    return 0;
}

PyObject* sbn::python::py_kernel_map_set_kernel_cpp(py_kernel_map* self, PyObject * args)
{
    PyObject* capsule_kernel_cpp;
    PyArg_UnpackTuple(args, "ptr", 0, 1, &capsule_kernel_cpp);

    if (!PyCapsule_IsValid(capsule_kernel_cpp, "ptr"))
        return nullptr;

    Py_INCREF(capsule_kernel_cpp);
    self->_kernel_map_capsule = capsule_kernel_cpp;

    Py_RETURN_NONE;
}

PyObject* sbn::python::py_kernel_map_reduce(py_kernel_map* self, PyObject* Py_UNUSED(ignored))
{
    PyObject * dict = PyObject_GetAttrString((PyObject*)self, "__dict__");
    return Py_BuildValue("N()N", Py_TYPE(self), dict);
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

void sbn::python::kernel_map::write(sbn::kernel_buffer& out) const {
    sys::log_message(">>>> Sbn", "Kernel_map.write");
    kernel::write(out); 

    PyObject *pickle = PyImport_ImportModule("pickle"); // import module
    Py_INCREF(pickle);

    PyObject *pickled_pyobj = PyObject_CallMethod(pickle, "dumps", "O", this->_py_k_map);
    Py_INCREF(pickled_pyobj);

    PyObject* pybytearr_pyobj = PyByteArray_FromObject(pickled_pyobj);
    Py_INCREF(pybytearr_pyobj);
    
    const char* bytearr_pyobj = PyByteArray_AsString(pybytearr_pyobj);
    Py_ssize_t size_pyobj = PyByteArray_Size(pybytearr_pyobj);
    std::string str_pyobj(bytearr_pyobj, size_pyobj);

    out << str_pyobj;
    out << size_pyobj;

    Py_XDECREF(pickle);
    Py_XDECREF(pickled_pyobj);
    Py_XDECREF(pybytearr_pyobj);
}

void sbn::python::kernel_map::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Kernel_map.read");
    kernel::read(in);
    PyObject *pickle = PyImport_ImportModule("pickle"); // import module
    Py_INCREF(pickle);

    std::string str_pyobj;
    Py_ssize_t size_pyobj;

    in >> str_pyobj;
    in >> size_pyobj;

    const char* bytearr_pyobj = str_pyobj.data();
    PyObject* pybytearr_pyobj = PyByteArray_FromStringAndSize(bytearr_pyobj, size_pyobj);
    Py_INCREF(pybytearr_pyobj);
    
    PyObject* pyobj = PyObject_CallMethod(pickle, "loads", "O", pybytearr_pyobj);
    Py_INCREF(pyobj);
    this->py_k_map(pyobj);
    PyObject_CallMethod(pyobj, "_set_kernel_cpp", "O", PyCapsule_New((void *)this, "ptr", nullptr));
    
    Py_XDECREF(pickle);
    Py_XDECREF(pybytearr_pyobj);
    Py_XDECREF(pyobj);
}


void sbn::python::Main::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Main.read");
    sys::log_message("test", "Main.read");
    sbn::kernel::read(in);
    if (in.remaining() != 0)
    {
        PyObject *pickle = PyImport_ImportModule("pickle"); // import module

        std::string str_pyobj;
        Py_ssize_t size_pyobj;

        in >> str_pyobj;
        in >> size_pyobj;

        const char* bytearr_pyobj = str_pyobj.data();
        PyObject* pybytearr_pyobj = PyByteArray_FromStringAndSize(bytearr_pyobj, size_pyobj);
        Py_INCREF(pybytearr_pyobj);

        PyObject* pyobj = PyObject_CallMethod(pickle, "loads", "O", pybytearr_pyobj);
        Py_INCREF(pyobj);

        this->py_k_map(pyobj);
        PyObject_CallMethod(pyobj, "_set_kernel_cpp", "O", PyCapsule_New((void *)this, "ptr", nullptr));

        Py_XDECREF(pickle);
        Py_XDECREF(pybytearr_pyobj);
        Py_XDECREF(pyobj);
    }
}

void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");
    sys::log_message("test", "Main.act");
    if (target_application()) {
        object main_module = PyImport_Import(object(PyUnicode_DecodeFSDefault("__main__")).get());
        if (main_module) {
            object main_class = PyObject_CallMethod(
                main_module, "Main", "O", PyCapsule_New((void *)this, "ptr", nullptr));
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
    }
}
