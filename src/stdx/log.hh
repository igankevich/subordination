#ifndef STDX_LOG_HH
#define STDX_LOG_HH

#include <typeinfo>
#include <type_traits>
#include <iostream>

namespace stdx {

	struct no_category {};

	template<class T>
	struct type_traits {
		static constexpr const char*
		short_name() noexcept {
			return typeid(T).name();
		}
		typedef no_category category;
	};

	template<class Category>
	struct disable_log_category:
	public std::integral_constant<bool, false> {};

	struct no_log {

		template<class T>
		constexpr const no_log&
		operator<<(const T&) const { return *this; }

		constexpr const no_log&
		operator<<(std::ostream& ( *pf )(std::ostream&)) const { return *this; }
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
	std::conditional<disable_log_category<typename type_traits<T>::category>::value,
	no_log, basic_log<T>>::type {};


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
		Out out;
		out << make_func(func, args...) << std::endl;
	}

}

#endif // STDX_LOG_HH
