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

PyObject* sbn::python::mutex_lock(Py_kernel* self, PyObject* Py_UNUSED(ignored)){
    sys::log_message(">>>> Sbn", "mutex_lock");
    python_mutex.lock();
    Py_RETURN_NONE;
}

PyObject* sbn::python::mutex_unlock(Py_kernel* self, PyObject* Py_UNUSED(ignored)){
    sys::log_message(">>>> Sbn", "mutex_unlock");
    python_mutex.unlock();
    Py_RETURN_NONE;
}

PyObject* sbn::python::sleep(Py_kernel* self, PyObject *args){
    int msecs = 0;
    if (!PyArg_ParseTuple(args, "i", &msecs))
        return nullptr;
    sys::log_message(">>>> Sbn", "sleep on _ milliseconds", msecs);

    Python_unlock unlock;
    std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
    Py_RETURN_NONE;
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
        self->_cpp_kernel_capsule = PyCapsule_New(cpp_kernel, "ptr", nullptr);
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

PyObject* sbn::python::Py_kernel_enable_carries_parent(Py_kernel* self, PyObject* Py_UNUSED(ignored))
{
    auto cpp_kernel = (sbn::python::Cpp_kernel*)PyCapsule_GetPointer(self->_cpp_kernel_capsule, "ptr");
    cpp_kernel->setf(sbn::kernel_flag::carries_parent);
    Py_RETURN_NONE;
}


void sbn::python::Cpp_kernel::act() {
    sys::log_message(">>>> Sbn", "Cpp_kernel.act");
    sys::log_message("test", "Sbn: Cpp_kernel.act");
    Python_lock lock;
    this->_py_kernel_obj.call("act", nullptr);
}

void sbn::python::Cpp_kernel::react(sbn::kernel_ptr&& cpp_child_ptr){
    sys::log_message(">>>> Sbn", "Cpp_kernel.react");
    sys::log_message("test", "Sbn: Cpp_kernel.react");
    Python_lock lock;
    auto cpp_child = sbn::pointer_dynamic_cast<Cpp_kernel>(std::move(cpp_child_ptr));
    this->_py_kernel_obj.call("react", "O", cpp_child->py_kernel_obj());
}

void sbn::python::Cpp_kernel::write(sbn::kernel_buffer& out) const {
    sys::log_message(">>>> Sbn", "Cpp_kernel.write");
    sys::log_message("test", "Sbn: Cpp_kernel.write");
    Python_lock lock;
    sys::log_message(">>>> Sbn", "Cpp_kernel.write start");
    kernel::write(out);
    object pickle_module = PyImport_ImportModule("pickle"); // import module
    object pkl_py_kernel_obj = pickle_module.call("dumps", "O", this->_py_kernel_obj.get());
    byte_array bytes{pkl_py_kernel_obj.get()};
    const int64_t n = bytes.size();
    out.write(n);
    out.write(bytes.data(), n);
}

void sbn::python::Cpp_kernel::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Cpp_kernel.read");
    sys::log_message("test", "Sbn: Cpp_kernel.read");
    Python_lock lock;
    kernel::read(in);
    object pickle_module = PyImport_ImportModule("pickle"); // import module
    byte_array bytes;
    int64_t n = 0;
    in.read(n);
    bytes.resize(n);
    in.read(bytes.data(), n);
    sys::log_message("python", "size _ kernel _", n, *this);
    py_kernel_obj(pickle_module.call("loads", "O", bytes.get()));
    this->_py_kernel_obj.call("_set_Cpp_kernel", "O", PyCapsule_New(this, "ptr", nullptr));
}


void sbn::python::Main::read(sbn::kernel_buffer& in) {
    sys::log_message(">>>> Sbn", "Main.read");
    sys::log_message("test", "Sbn: Main.read");
    Python_lock lock;
    sbn::kernel::read(in);
    if (in.remaining() != 0) {
        object pickle_module = PyImport_ImportModule("pickle"); // import module
        byte_array bytes;
        int64_t n = 0;
        in.read(n);
        bytes.resize(n);
        in.read(bytes.data(), n);
        py_kernel_obj(pickle_module.call("loads", "O", bytes.get()));
        this->_py_kernel_obj.call("_set_Cpp_kernel", "O", PyCapsule_New(this, "ptr", nullptr));
    }
}

void sbn::python::Main::act() {
    sys::log_message(">>>> Sbn", "Main.act");
    sys::log_message("test", "Sbn: Main.act");
    Python_lock lock;
    if (target_application()) {
        object main_module = PyImport_Import(object(PyUnicode_DecodeFSDefault("__main__")).get());
        if (!main_module) {
            PyErr_Print();
            throw std::runtime_error("failed to find __main__ module");
        }
        py_kernel_obj(main_module.call("Main", "O", PyCapsule_New(this, "ptr", nullptr)));
        this->_py_kernel_obj.call("act", nullptr);
    }
}
