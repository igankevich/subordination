#ifndef SUBORDINATION_PYTHON_PYTHON_HH
#define SUBORDINATION_PYTHON_PYTHON_HH

#include <Python.h>

#include <memory>

namespace sbn {
    namespace python {

        class interpreter_guard {
        public:
            inline interpreter_guard() noexcept { ::Py_InitializeEx(0); }
            inline ~interpreter_guard() noexcept { ::Py_FinalizeEx(); }
            interpreter_guard(const interpreter_guard&) = delete;
            interpreter_guard& operator=(const interpreter_guard&) = delete;
            interpreter_guard(interpreter_guard&&) = delete;
            interpreter_guard& operator=(interpreter_guard&&) = delete;
        };

        class thread_guard {
        private:
            ::PyThreadState* _state;
        public:
            inline thread_guard() noexcept {
            //    PyEval_InitThreads();
                _state = ::PyEval_SaveThread();
            }
            inline ~thread_guard() noexcept { ::PyEval_RestoreThread(this->_state); }
            thread_guard(const thread_guard&) = delete;
            thread_guard& operator=(const thread_guard&) = delete;
            thread_guard(thread_guard&&) = delete;
            thread_guard& operator=(thread_guard&&) = delete;
        };

        class gil_guard: public thread_guard {
        private:
            ::PyGILState_STATE _state;
        public:
            inline gil_guard() noexcept: _state{::PyGILState_Ensure()} {}
            inline ~gil_guard() noexcept { ::PyGILState_Release(this->_state); }
            gil_guard(const gil_guard&) = delete;
            gil_guard& operator=(const gil_guard&) = delete;
            gil_guard(gil_guard&&) = delete;
            gil_guard& operator=(gil_guard&&) = delete;
        };

        struct python_pointer_deleter {
            inline void operator()(void* ptr) { ::PyMem_RawFree(ptr); }
        };

        template <class T>
        using pointer = std::unique_ptr<T,python_pointer_deleter>;

        void set_arguments(int argc, char** argv);
        void load(const char* filename);

        class object {

        private:
            mutable PyObject* _ptr{};

        public:
            object() noexcept = default;
            inline object(PyObject* ptr) noexcept: _ptr(ptr) {}
            inline ~object() noexcept { release(); }
            inline object(const object& rhs) noexcept: _ptr(rhs._ptr) { retain(); }
            inline object& operator=(const object& rhs) noexcept { object tmp(rhs); swap(tmp); return *this; }
            inline object(object&& rhs) noexcept: _ptr(rhs._ptr) { rhs._ptr = nullptr; }
            inline object& operator=(object&& rhs) noexcept { swap(rhs); return *this; };
            inline void swap(object& rhs) noexcept { std::swap(this->_ptr, rhs._ptr); }
            inline explicit operator bool() const noexcept { return this->_ptr != nullptr; }
            inline bool operator !() const noexcept { return !this->operator bool(); }
            inline PyObject* get() noexcept { return this->_ptr; }
            inline PyObject* get() const noexcept { return this->_ptr; }
            inline operator PyObject*() noexcept { return get(); }
            inline operator const PyObject*() const noexcept { return get(); }
            inline Py_ssize_t size() const noexcept { return Py_SIZE(this->_ptr); }
            inline const PyTypeObject* type() const noexcept { return Py_TYPE(this->_ptr); }
            inline PyTypeObject* type() noexcept { return Py_TYPE(this->_ptr); }
            inline Py_ssize_t reference_count() const noexcept { return Py_REFCNT(this->_ptr); }
            inline void clear() noexcept { Py_CLEAR(this->_ptr); }
            inline void retain() noexcept { Py_XINCREF(this->_ptr); }
            inline void release() noexcept { Py_XDECREF(this->_ptr); }
            inline PyObject* disown() noexcept {
                auto tmp = this->_ptr;
                this->_ptr = nullptr;
                return tmp;
            }
            //inline void reference_count(Py_ssize_t n) noexcept { Py_SET_REFCNT(this->_ptr, n); }

            template <class ... Args>
            inline object call(const char* name, const char* format, Args ... args) {
                return ::PyObject_CallMethod(this->_ptr, name, format, args...);
            }

        };

        inline void swap(object& a, object& b) { a.swap(b); }

        class byte_array: public object {

        public:
            inline byte_array() noexcept:
            byte_array(nullptr, 0) {}

            inline explicit byte_array(const char* data, Py_ssize_t size) noexcept:
            object(::PyByteArray_FromStringAndSize(data, size)) {}

            inline explicit byte_array(PyObject* obj) noexcept:
            object(::PyByteArray_FromObject(obj)) {}

            inline const char* data() const noexcept { return ::PyByteArray_AsString(get()); }
            inline char* data() noexcept { return ::PyByteArray_AsString(get()); }
            inline Py_ssize_t size() const noexcept {return ::PyByteArray_Size(get());}
            inline void resize(Py_ssize_t n) noexcept { ::PyByteArray_Resize(get(), n); }

        };

    }
}

#endif // vim:filetype=cpp
