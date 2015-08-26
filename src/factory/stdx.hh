#ifndef FACTORY_STDX_HH
#define FACTORY_STDX_HH

#include "bits/stdx.hh"
#include "bits/uint128.hh"

#include "stdx/n_random_bytes.hh"
#include "stdx/intersperse_iterator.hh"
#include "stdx/front_popper.hh"
#include "stdx/back_inserter.hh"
#include "stdx/paired_iterator.hh"
#include "stdx/field_iterator.hh"
#include "stdx/spin_mutex.hh"
#include "stdx/iosx.hh"

namespace factory {

	namespace stdx {

		// TODO: deprecated
		struct use_flags {
			explicit
			use_flags(std::ios_base& s):
				str(s), oldf(str.flags()) {}
			template<class ... Args>
			use_flags(std::ios_base& s, Args&& ... args):
				str(s), oldf(str.setf(std::forward<Args>(args)...)) {}
			~use_flags() {
				str.setf(oldf);
			}
		private:
			std::ios_base& str;
			std::ios_base::fmtflags oldf;
		};

		template<size_t No, class F>
		constexpr bits::Apply_to<No,F>
		apply_to(F&& f) {
			return bits::Apply_to<No,F>(f);
		}

		template<class It, class Pred, class Func>
		void
		for_each_if(It first, It last, Pred pred, Func func) {
			while (first != last) {
				if (pred(*first)) {
					func(*first);
				}
				++first;
			}
		}

		template<class Lock, class It, class Func>
		void
		for_each_thread_safe(Lock& lock, It first, It last, Func func) {
			while (first != last) {
				lock.unlock();
				func(*first);
				lock.lock();
				++first;
			}
		}

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

}
#endif // FACTORY_STDX_HH
