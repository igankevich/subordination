#ifndef STDX_LOG_HH
#define STDX_LOG_HH

#include <typeinfo>
#include <type_traits>
#include <iostream>

#if defined(__GLIBCXX__) || defined(__GLIBCPP__) || (defined(_LIBCPP_VERSION) && defined(__APPLE__))
#define FACTORY_TEST_HAVE_CXXABI
#endif

#if defined(FACTORY_TEST_HAVE_CXXABI)
#include <cxxabi.h>
#endif

namespace stdx {

	template<class T>
	std::string
	demangle_name() {
		#if defined(FACTORY_TEST_HAVE_CXXABI)
		int status;
		return std::string(abi::__cxa_demangle(typeid(T).name(), 0, 0, &status));
		#else
		return std::string(typeid(T).name());
		#endif
	}

	struct no_category {};

	template<class T>
	struct type_traits {
		static constexpr const char*
		short_name() noexcept {
			return _name.data();
		}
		typedef no_category category;
	private:
		static std::string _name;
	};

	template<class T>
	std::string type_traits<T>::_name = demangle_name<T>();

	/// disable all debug logs by default
	template<class Category>
	struct enable_log: public std::false_type {};

	struct no_log {

		template<class T>
		inline constexpr const no_log&
		operator<<(const T&) const noexcept {
			return *this;
		}

		inline constexpr const no_log&
		operator<<(std::ostream& (*pf)(std::ostream&)) const noexcept {
			return *this;
		}

	};

	#if defined(FACTORY_DISABLE_LOGS)
	template<class Tp> struct basic_log: public no_log {};
	#else
	template<class Tp>
	struct basic_log {

		basic_log() {
			write_header(std::clog,
			stdx::type_traits<Tp>::short_name());
		}

		constexpr basic_log(const basic_log&) {}
		basic_log& operator=(const basic_log&) = delete;

		template<class T>
		basic_log<Tp>&
		operator<<(const T& rhs) {
			std::clog << rhs;
			return *this;
		}

		basic_log<Tp>&
		operator<<(std::ostream& (*rhs)(std::ostream&)) {
			std::clog << rhs;
			return *this;
		}

	private:

		static void
		write_header(std::ostream& out, const char* func) {
			if (func) {
				out << func << ' ';
			}
		}

	};
	#endif

	template<class T>
	struct log: public
	std::conditional<
		enable_log<typename type_traits<T>::category>::value,
		basic_log<T>,
		no_log
	>::type
	{};


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
				return out << static_cast<Field<Args...>>(rhs)
					<< ',' << rhs._key << '=' << rhs._val;
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

	}

	template<class ... Args>
	bits::Field<Args...>
	make_fields(const Args& ... args) {
		return bits::Field<Args...>(args...);
	}

	template<class ... Args>
	bits::Function<Args...>
	make_func(const char* name, const Args& ... args) {
		return bits::Function<Args...>(name, args...);
	}

	std::ostream&
	format_fields_impl(std::ostream& out) {
		return out;
	}

	template<class K, class V, class ... Args>
	std::ostream&
	format_fields_impl(std::ostream& out, const K& key, const V& value, const Args& ... args) {
		out << ',' << key << '=' << value;
		return format_fields_impl(out, args...);
	}

	template<class K, class V, class ... Args>
	std::ostream&
	format_fields(std::ostream& out, const K& key, const V& value, const Args& ... args) {
		out << '{';
		out << key << '=' << value;
		format_fields_impl(out, args...);
		out << '}';
		return out;
	}

	template<class Out, class ... Args>
	void
	log_func(const char* func, const Args& ... args) {
		Out() << make_func(func, args...) << std::endl;
	}

}

#endif // STDX_LOG_HH
