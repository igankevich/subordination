#ifndef STDX_LOG_HH
#define STDX_LOG_HH

#include <iostream>
#include <iomanip>
#include <mutex>

namespace stdx {

	class debug_message;

	/**
		Ideal debug log
		- is thread-safe,
		- supports class and object tags to trace messages of specific classes/objects,
		- can be enabled/disabled compile-time,
		- attaches to *any* existing output stream,
		- each message occupies a single line.
	*/
	class debug_log {

		typedef std::recursive_mutex mutex_type;
		typedef std::unique_lock<mutex_type> lock_type;

	public:

		explicit
		debug_log(std::ostream& str) noexcept:
		_out(str)
		{}

		~debug_log() = default;
		debug_log(const debug_log&) = delete;
		debug_log& operator=(const debug_log&) = delete;

		template<class T>
		debug_log&
		operator<<(const T& rhs) {
			lock_type lock(_mutex);
			_out << rhs << std::endl;
			return *this;
		}

		friend class debug_message;

	private:

		std::ostream& _out;
		mutex_type _mutex;

	};

	debug_log dbg(std::clog);

	class debug_message {

		typedef debug_log::mutex_type mutex_type;
		typedef debug_log::lock_type lock_type;

	public:

		explicit
		debug_message(debug_log& rhs, const char* name, const char* msg=nullptr):
		_log(rhs)
		{
			_log._mutex.lock();
			_log._out << std::setw(10) << std::right << name << ": ";
			if (msg) {
				_log._out << msg << ' ';
			}
		}

		explicit
		debug_message(const char* name, const char* msg=nullptr):
		debug_message(::stdx::dbg, name, msg)
		{}

		~debug_message() {
			_log._out << std::endl;
			_log._mutex.unlock();
		}

		template<class T>
		std::ostream&
		operator<<(const T& rhs) {
			return _log._out << rhs;
		}

		std::ostream&
		operator<<(std::ostream& (*rhs)(std::ostream&)) {
			return _log._out << rhs;
		}

		std::ostream&
		out() noexcept {
			return _log._out;
		}

	private:

		debug_log& _log;

	};

	namespace bits {

		template<class ... Args> struct Field {
			friend std::ostream&
			operator<<(std::ostream& out, const Field&) {
				return out;
			}
		};

		template<class Key, class Value>
		struct Field<Key, Value> {

			Field(const Key& k, const Value& v):
			_key(k), _val(v)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Field& rhs) {
				return out << rhs._key << '=' << rhs._val;
			}

		private:
			const Key& _key;
			const Value& _val;
		};

		template<class Key, class Value, class ... Args>
		struct Field<Key, Value, Args...>: public Field<Args...> {

			Field(const Key& k, const Value& v, const Args& ... args):
			Field<Args...>(args...),
			_key(k), _val(v)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Field& rhs) {
				return out << rhs._key << '=' << rhs._val << ','
					<< static_cast<Field<Args...>>(rhs);
			}

		private:
			const Key& _key;
			const Value& _val;
		};

		template<class ... Args>
		struct Function: public Field<Args...> {

			explicit
			Function(const char* name, const Args& ... args):
			Field<Args...>(args...),
			_name(name)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Function& rhs) {
				return out << rhs._name << '(' << static_cast<Field<Args...>>(rhs) << ')';
			}

		private:
			const char* _name;
		};

		template<class T>
		struct Object {

			explicit
			Object(const T& rhs):
			_obj(rhs)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Object& rhs) {
				return out << rhs._obj;
			}

		private:

			const T& _obj;

		};

	}

	template<class ... Args>
	bits::Field<Args...>
	make_fields(const Args& ... args) {
		return bits::Field<Args...>(args...);
	}

	template<class ... Args>
	bits::Object<bits::Field<Args...>>
	make_object(const Args& ... args) {
		return bits::Object<bits::Field<Args...>>(make_fields(args...));
	}

	template<class ... Args>
	bits::Function<Args...>
	make_func(const char* name, const Args& ... args) {
		return bits::Function<Args...>(name, args...);
	}

}

#endif // STDX_LOG_HH
