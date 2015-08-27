#ifndef FACTORY_STDX_LOG_HH
#define FACTORY_STDX_LOG_HH

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

		basic_log(const basic_log&) = delete;
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

}

#endif // FACTORY_STDX_LOG_HH
