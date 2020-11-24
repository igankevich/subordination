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

PyObject* sbn::python::upstream(PyObject *self, PyObject *args, PyObject *kwds){
    sys::log_message(">>>> Sbn", "upstream");
    sys::log_message("test", "Sbn: upstream");
    PyObject *py_kernel_obj_parent = nullptr, *py_kernel_obj_child = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!O!", const_cast<char**>(upstream_kwlist),
        &Py_kernel_type,
        &py_kernel_obj_parent,
        &Py_kernel_type,
        &py_kernel_obj_child))
        return nullptr;

    Py_INCREF(py_kernel_obj_parent);
    Py_INCREF(py_kernel_obj_child);

    auto py_kernel_parent = (Py_kernel*)py_kernel_obj_parent;
    auto py_kernel_child = (Py_kernel*)py_kernel_obj_child;

    auto cpp_kernel_parent = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel_parent->_cpp_kernel_capsule, "ptr");
    auto cpp_kernel_child = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel_child->_cpp_kernel_capsule, "ptr");

    sbn::upstream<sbn::Remote>(std::move(cpp_kernel_parent),
                               std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel_child)));

    Py_RETURN_NONE;
}

PyObject* sbn::python::commit(PyObject *self, PyObject *args, PyObject *kwds) {
    sys::log_message(">>>> Sbn", "commit");
    sys::log_message("test", "Sbn: commit");
    PyObject *py_kernel_obj = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!", const_cast<char**>(commit_kwlist),
        &Py_kernel_type,
        &py_kernel_obj
        ))
    {
        return nullptr;
    }
    Py_INCREF(py_kernel_obj);

    auto py_kernel = (Py_kernel*)py_kernel_obj;

    auto cpp_kernel = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel->_cpp_kernel_capsule, "ptr");

    sbn::commit<sbn::Remote>(std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel)));
    Py_RETURN_NONE;
}


void sbn::python::Py_kernel_dealloc(Py_kernel* self)
{
    /* Custom deallocation behavior */

    // ...

    // Default deallocation behavior
    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* sbn::python::Py_kernel_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    /* Custom allocation behavior */

    // Default allocation behavior
    auto self = (Py_kernel *) type->tp_alloc(type, 0);

    return (PyObject *) self;
}

int sbn::python::Py_kernel_init(Py_kernel* self, PyObject* args, PyObject* kwds)
{
    /* Initialization of kernel */
    PyObject* cpp_kernel_capsule = nullptr;
    PyArg_UnpackTuple(args, "ptr", 0, 1, &cpp_kernel_capsule);

    if (PyCapsule_IsValid(cpp_kernel_capsule, "ptr"))
    {
        sys::log_message("test", "Sbn: Py_kernel_main.__init__");
        Py_INCREF(cpp_kernel_capsule);
        self->_cpp_kernel_capsule = cpp_kernel_capsule;
    }
    else
    {
        auto cpp_kernel = new Cpp_kernel(std::move((PyObject*)self));
        self->_cpp_kernel_capsule = PyCapsule_New((void *)cpp_kernel, "ptr", nullptr);
    }

    return 0;
}

PyObject* sbn::python::Py_kernel_set_Cpp_kernel(Py_kernel* self, PyObject * args)
{
    PyObject* cpp_kernel_capsule = nullptr;
    PyArg_UnpackTuple(args, "ptr", 0, 1, &cpp_kernel_capsule);

    if (!PyCapsule_IsValid(cpp_kernel_capsule, "ptr"))
        return nullptr;

    Py_INCREF(cpp_kernel_capsule);
    self->_cpp_kernel_capsule = cpp_kernel_capsule;

    Py_RETURN_NONE;
}

PyObject* sbn::python::Py_kernel_reduce(Py_kernel* self, PyObject* Py_UNUSED(ignored))
{
    PyObject * dict = PyObject_GetAttrString((PyObject*)self, "__dict__");
    return Py_BuildValue("N()N", Py_TYPE(self), dict);
}


void sbn::python::Cpp_kernel::act() {
    sys::log_message(">>>> Sbn", "Cpp_kernel.act");
    sys::log_message("test", "Sbn: Cpp_kernel.act");
    object pValue = PyObject_CallMethod(this->py_kernel_obj(), "act", nullptr);
    if (!pValue) {
        PyErr_Print();
        sbn::exit(1);
    }
}

void sbn::python::Cpp_kernel::react(sbn::kernel_ptr&& cpp_child_ptr){
    sys::log_message(">>>> Sbn", "Cpp_kernel.react");
    sys::log_message("test", "Sbn: Cpp_kernel.react");
    auto cpp_child = sbn::pointer_dynamic_cast<Cpp_kernel>(std::move(cpp_child_ptr));
    object pValue = PyObject_CallMethod(this->py_kernel_obj(), "react", "O", cpp_child->py_kernel_obj());
    if (!pValue) {
        PyErr_Print();
        sbn::exit(1);
    }
}

void sbn::python::Cpp_kernel::write(sbn::kernel_buffer& out) const {
    sys::log_message(">>>> Sbn", "Cpp_kernel.write");
    sys::log_message("test", "Sbn: Cpp_kernel.write");
    kernel::write(out); 

    PyObject *pickle_module = PyImport_ImportModule("pickle"); // import module
    Py_INCREF(pickle_module);

    PyObject *pkl_py_kernel_obj = PyObject_CallMethod(pickle_module, "dumps", "O", this->_py_kernel_obj);
    Py_INCREF(pkl_py_kernel_obj);

    PyObject* py_bytearr_py_kernel_obj = PyByteArray_FromObject(pkl_py_kernel_obj);
    Py_INCREF(py_bytearr_py_kernel_obj);
    
    const char* bytearr_py_kernel_obj = PyByteArray_AsString(py_bytearr_py_kernel_obj);
    Py_ssize_t size_py_kernel_obj = PyByteArray_Size(py_bytearr_py_kernel_obj);
    std::string str_py_kernel_obj(bytearr_py_kernel_obj, size_py_kernel_obj);

    out << str_py_kernel_obj;
    out << size_py_kernel_obj;

    Py_XDECREF(pickle_module);
    Py_XDECREF(pkl_py_kernel_obj);
    Py_XDECREF(py_bytearr_py_kernel_obj);
}

void sbn::python::Cpp_kernel::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Cpp_kernel.read");
    sys::log_message("test", "Sbn: Cpp_kernel.read");
    kernel::read(in);

    PyObject *pickle_module = PyImport_ImportModule("pickle"); // import module
    Py_INCREF(pickle_module);

    std::string str_py_kernel_obj;
    Py_ssize_t size_py_kernel_obj;
    in >> str_py_kernel_obj;
    in >> size_py_kernel_obj;

    const char* bytearr_py_kernel_obj = str_py_kernel_obj.data();
    PyObject* py_bytearr_py_kernel_obj = PyByteArray_FromStringAndSize(bytearr_py_kernel_obj, size_py_kernel_obj);
    Py_INCREF(py_bytearr_py_kernel_obj);
    
    PyObject* py_kernel_obj = PyObject_CallMethod(pickle_module, "loads", "O", py_bytearr_py_kernel_obj);
    Py_INCREF(py_kernel_obj);

    this->py_kernel_obj(py_kernel_obj);
    PyObject_CallMethod(py_kernel_obj, "_set_Cpp_kernel", "O", PyCapsule_New((void *)this, "ptr", nullptr));
    
    Py_XDECREF(pickle_module);
    Py_XDECREF(py_bytearr_py_kernel_obj);
    Py_XDECREF(py_kernel_obj);
}


void sbn::python::Main::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Main.read");
    sys::log_message("test", "Sbn: Main.read");
    sbn::kernel::read(in);

    if (in.remaining() != 0)
    {
        PyObject *pickle_module = PyImport_ImportModule("pickle"); // import module
        Py_INCREF(pickle_module);

        std::string str_py_kernel_obj;
        Py_ssize_t size_py_kernel_obj;
        in >> str_py_kernel_obj;
        in >> size_py_kernel_obj;

        const char* bytearr_py_kernel_obj = str_py_kernel_obj.data();
        PyObject* py_bytearr_py_kernel_obj = PyByteArray_FromStringAndSize(bytearr_py_kernel_obj, size_py_kernel_obj);
        Py_INCREF(py_bytearr_py_kernel_obj);
        
        PyObject* py_kernel_obj = PyObject_CallMethod(pickle_module, "loads", "O", py_bytearr_py_kernel_obj);
        Py_INCREF(py_kernel_obj);

        this->py_kernel_obj(py_kernel_obj);
        PyObject_CallMethod(py_kernel_obj, "_set_Cpp_kernel", "O", PyCapsule_New((void *)this, "ptr", nullptr));
        
        Py_XDECREF(pickle_module);
        Py_XDECREF(py_bytearr_py_kernel_obj);
        Py_XDECREF(py_kernel_obj);
    }
}

void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");
    sys::log_message("test", "Sbn: Main.act");
    if (target_application()) {
        object main_module = PyImport_Import(object(PyUnicode_DecodeFSDefault("__main__")).get());
        if (main_module) {
            object py_main_obj = PyObject_CallMethod(
                main_module, "Main", "O", PyCapsule_New((void *)this, "ptr", nullptr));
            this->py_kernel_obj(py_main_obj);
            object pValue = PyObject_CallMethod(py_main_obj, "act", nullptr);
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
