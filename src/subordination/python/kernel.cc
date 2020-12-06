#include <sstream>
#include <typeinfo>

#include <subordination/api.hh>
#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>
#include <subordination/python/python.hh>

namespace {
    constexpr const char* upstream_kwlist[] = {"parent", "child", "target", nullptr};
    constexpr const char* commit_kwlist[] = {"kernel", "target", nullptr};

    std::mutex python_mutex;

    class Python_lock {
    public:
        inline Python_lock() noexcept { python_mutex.lock(); }
        inline ~Python_lock() noexcept { python_mutex.unlock(); }
        Python_lock(const Python_lock&) = delete;
        Python_lock& operator=(const Python_lock&) = delete;
        Python_lock(Python_lock&&) = delete;
        Python_lock& operator=(Python_lock&&) = delete;
    };

    class Python_unlock {
    public:
        inline Python_unlock() noexcept { python_mutex.unlock(); }
        inline ~Python_unlock() noexcept { python_mutex.lock(); }
        Python_unlock(const Python_unlock&) = delete;
        Python_unlock& operator=(const Python_unlock&) = delete;
        Python_unlock(Python_unlock&&) = delete;
        Python_unlock& operator=(Python_unlock&&) = delete;
    };

}

PyObject* sbn::python::upstream(PyObject *self, PyObject *args, PyObject *kwds){
    sys::log_message(">>>> Sbn", "upstream");
    sys::log_message("test", "Sbn: upstream");
    int target = 0;
    PyObject *py_kernel_obj_parent = nullptr, *py_kernel_obj_child = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!O!$i", const_cast<char**>(upstream_kwlist),
        &Py_kernel_type,
        &py_kernel_obj_parent,
        &Py_kernel_type,
        &py_kernel_obj_child,
        &target))
        return nullptr;

    Py_INCREF(py_kernel_obj_parent);
    Py_INCREF(py_kernel_obj_child);

    auto py_kernel_parent = (Py_kernel*)py_kernel_obj_parent;
    auto py_kernel_child = (Py_kernel*)py_kernel_obj_child;

    auto cpp_kernel_parent = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel_parent->_cpp_kernel_capsule, "ptr");
    auto cpp_kernel_child = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel_child->_cpp_kernel_capsule, "ptr");

    Python_unlock unlock;
    if (static_cast<sbn::Target>(target) == sbn::Remote)
        sbn::upstream<sbn::Remote>(std::move(cpp_kernel_parent),
                                std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel_child)));
    else
        sbn::upstream<sbn::Local>(std::move(cpp_kernel_parent),
                                std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel_child)));

    Py_RETURN_NONE;
}

PyObject* sbn::python::commit(PyObject *self, PyObject *args, PyObject *kwds) {
    sys::log_message(">>>> Sbn", "commit");
    sys::log_message("test", "Sbn: commit");
    int target = 0;
    PyObject *py_kernel_obj = nullptr;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "|O!$i", const_cast<char**>(commit_kwlist),
        &Py_kernel_type,
        &py_kernel_obj,
        &target))
        return nullptr;

    Py_INCREF(py_kernel_obj);
    auto py_kernel = (Py_kernel*)py_kernel_obj;

    auto cpp_kernel = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(py_kernel->_cpp_kernel_capsule, "ptr");

    Python_unlock unlock;
    if (static_cast<sbn::Target>(target) == sbn::Remote)
        sbn::commit<sbn::Remote>(std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel)));
    else
        sbn::commit<sbn::Local>(std::unique_ptr<Cpp_kernel>(std::move(cpp_kernel)));
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
        auto cpp_kernel = new Cpp_kernel((PyObject*)self);
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
    object dict = PyObject_GetAttrString((PyObject*)self, "__dict__");
    return Py_BuildValue("N()N", Py_TYPE(self), dict.get());
}


void sbn::python::Cpp_kernel::act() {
    sys::log_message(">>>> Sbn", "Cpp_kernel.act");
    sys::log_message("test", "Sbn: Cpp_kernel.act");
    Python_lock lock;
    object pValue = PyObject_CallMethod(this->py_kernel_obj(), "act", nullptr);
    if (!pValue) {
        PyErr_Print();
        sbn::exit(1);
    }
}

void sbn::python::Cpp_kernel::react(sbn::kernel_ptr&& cpp_child_ptr){
    sys::log_message(">>>> Sbn", "Cpp_kernel.react");
    sys::log_message("test", "Sbn: Cpp_kernel.react");
    Python_lock lock;
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
    Python_lock lock;

    object pickle_module = PyImport_ImportModule("pickle"); // import module

    object pkl_py_kernel_obj = PyObject_CallMethod(pickle_module.get(), "dumps", "O", this->_py_kernel_obj);
    object py_bytearr_py_kernel_obj = PyByteArray_FromObject(pkl_py_kernel_obj.get());
    const char* bytearr_py_kernel_obj = PyByteArray_AsString(py_bytearr_py_kernel_obj.get());

    Py_ssize_t size_py_kernel_obj = PyByteArray_Size(py_bytearr_py_kernel_obj.get());
    std::string str_py_kernel_obj(bytearr_py_kernel_obj, size_py_kernel_obj);

    out << str_py_kernel_obj;
    out << size_py_kernel_obj;
}

void sbn::python::Cpp_kernel::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Cpp_kernel.read");
    sys::log_message("test", "Sbn: Cpp_kernel.read");
    kernel::read(in);
    Python_lock lock;

    object pickle_module = PyImport_ImportModule("pickle"); // import module

    std::string str_py_kernel_obj;
    Py_ssize_t size_py_kernel_obj;
    in >> str_py_kernel_obj;
    in >> size_py_kernel_obj;

    const char* bytearr_py_kernel_obj = str_py_kernel_obj.data();
    object py_bytearr_py_kernel_obj = PyByteArray_FromStringAndSize(bytearr_py_kernel_obj, size_py_kernel_obj);
    object py_kernel_obj = PyObject_CallMethod(pickle_module, "loads", "O", py_bytearr_py_kernel_obj.get());

    this->py_kernel_obj(py_kernel_obj.get());
    PyObject_CallMethod(py_kernel_obj.get(), "_set_Cpp_kernel", "O", PyCapsule_New((void *)this, "ptr", nullptr));
}


void sbn::python::Main::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Main.read");
    sys::log_message("test", "Sbn: Main.read");
    sbn::kernel::read(in);
    Python_lock lock;

    if (in.remaining() != 0)
    {
        object pickle_module = PyImport_ImportModule("pickle"); // import module

        std::string str_py_kernel_obj;
        Py_ssize_t size_py_kernel_obj;
        in >> str_py_kernel_obj;
        in >> size_py_kernel_obj;

        const char* bytearr_py_kernel_obj = str_py_kernel_obj.data();
        object py_bytearr_py_kernel_obj = PyByteArray_FromStringAndSize(bytearr_py_kernel_obj, size_py_kernel_obj);
        object py_kernel_obj = PyObject_CallMethod(pickle_module, "loads", "O", py_bytearr_py_kernel_obj.get());

        this->py_kernel_obj(py_kernel_obj.get());
        PyObject_CallMethod(py_kernel_obj.get(), "_set_Cpp_kernel", "O", PyCapsule_New((void *)this, "ptr", nullptr));
    }
}

void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");
    sys::log_message("test", "Sbn: Main.act");
    Python_lock lock;
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
