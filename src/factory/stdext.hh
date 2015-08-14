#ifndef FACTORY_STDEXT_HH
#define FACTORY_STDEXT_HH

#include "bits/basic_uint.hh"
#include "bits/uint128.hh"

namespace factory {

	namespace components {

		template <class T, class Ch=char, class Tr=std::char_traits<Ch>>
		struct intersperse_iterator:
			public std::iterator<std::output_iterator_tag, void, void, void, void>
		{
			typedef Ch char_type;
			typedef Tr traits_type;
			typedef std::basic_ostream<Ch,Tr> ostream_type;
			typedef const char_type* delim_type;

			explicit constexpr
			intersperse_iterator(ostream_type& s, delim_type delimiter=nullptr):
				ostr(&s), delim(delimiter) {}
			constexpr intersperse_iterator() = default;
			inline ~intersperse_iterator() = default;
			constexpr intersperse_iterator(const intersperse_iterator&) = default;
			inline intersperse_iterator(intersperse_iterator&& rhs):
				ostr(rhs.ostr), delim(rhs.delim), first(rhs.first)
			{ rhs.ostr = nullptr; }
			inline intersperse_iterator& operator=(const intersperse_iterator&) = default;
		
			inline intersperse_iterator& operator=(const T& value) {
				if (ostr) {
					if (delim != 0 && !first) { *ostr << delim; }
					*ostr << value;
					if (first) { first = false; }
				}
				return *this;
			}
		
			inline intersperse_iterator& operator*() { return *this; }
			inline intersperse_iterator& operator++() { return *this; }
			inline intersperse_iterator& operator++(int) { return *this; }

		private:
			ostream_type* ostr;
			delim_type delim = nullptr;
			bool first = true;
		};

		template<class It1, class It2>
		struct paired_iterator: public std::iterator<
			typename std::iterator_traits<It1>::iterator_category,
			std::tuple<typename std::iterator_traits<It1>::value_type,typename std::iterator_traits<It2>::value_type>,
			std::ptrdiff_t,
			std::tuple<typename std::iterator_traits<It1>::value_type*,typename std::iterator_traits<It2>::value_type*>,
			std::tuple<typename std::iterator_traits<It1>::value_type&&,typename std::iterator_traits<It2>::value_type&&>
			>
		{
			typedef typename std::iterator_traits<paired_iterator>::reference reference;
			typedef typename std::iterator_traits<paired_iterator>::pointer pointer;
			typedef typename std::iterator_traits<paired_iterator>::difference_type difference_type;
			typedef const reference const_reference;
			typedef const pointer const_pointer;

			typedef typename std::iterator_traits<It1>::value_type&& rvalueref1;
			typedef typename std::iterator_traits<It2>::value_type&& rvalueref2;

			inline paired_iterator(It1 x, It2 y): iter1(x), iter2(y) {}
			inline paired_iterator() = default;
			inline ~paired_iterator() = default;
			inline paired_iterator(const paired_iterator&) = default;
			inline paired_iterator& operator=(const paired_iterator&) = default;

			constexpr bool operator==(const paired_iterator& rhs) const { return iter1 == rhs.iter1; }
			constexpr bool operator!=(const paired_iterator& rhs) const { return !this->operator==(rhs); }

			inline const_reference operator*() const { return std::tuple<rvalueref1,rvalueref2>(*iter1,*iter2); }
			inline reference operator*() { return std::tuple<rvalueref1,rvalueref2>(std::move(*iter1),std::move(*iter2)); }
			inline const_pointer operator->() const { return std::make_tuple(iter1.operator->(),iter2.operator->()); }
			inline pointer operator->() { return std::make_tuple(iter1.operator->(),iter2.operator->()); }
			inline paired_iterator& operator++() { ++iter1; ++iter2; return *this; }
			inline paired_iterator operator++(int) { paired_iterator tmp(*this); ++iter1; ++iter2; return tmp; }
			inline difference_type operator-(const paired_iterator& rhs) const {
				return std::distance(rhs.iter1, iter1);
			}
			inline paired_iterator operator+(difference_type rhs) const {
				return paired_iterator(std::advance(iter1, rhs), std::advance(iter2, rhs));
			}
			inline It1 first() const { return iter1; }
			inline It2 second() const { return iter2; }

		private:
			It1 iter1;
			It2 iter2;
		};

		template<class It1, class It2>
		inline paired_iterator<It1,It2> make_paired(It1 it1, It2 it2) {
			return paired_iterator<It1,It2>(it1, it2);
		}

		template<size_t No, class F>
		struct Apply_to {
			constexpr explicit Apply_to(F&& f): func(f) {}
			template<class Arg>
			auto operator()(const Arg& rhs) ->
				typename std::result_of<F&(decltype(std::get<No>(rhs)))>::type
			{
				return func(std::get<No>(rhs));
			}
		private:
			F&& func;
		};

		template<size_t No, class F>
		constexpr Apply_to<No,F>
		apply_to(F&& f) {
			return Apply_to<No,F>(f);
		}

		template<class Result, class Engine>
		Result n_random_bytes(Engine& engine) {
			typedef Result result_type;
			typedef typename Engine::result_type
				base_result_type;
			static_assert(sizeof(base_result_type) > 0 &&
				sizeof(result_type) > 0 &&
				sizeof(result_type) % sizeof(base_result_type) == 0,
				"bad result type");
			constexpr const std::size_t NUM_BASE_RESULTS =
				sizeof(result_type) / sizeof(base_result_type);
			union {
				result_type value{};
				base_result_type base[NUM_BASE_RESULTS];
			} result;
			std::generate_n(result.base,
				NUM_BASE_RESULTS,
				std::ref(engine));
			return result.value;
		}

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

		/// Fast mutex for scheduling strategies.
		struct spin_mutex {
	
			inline void
			lock() noexcept {
				while (_flag.test_and_set(std::memory_order_acquire));
			}

			inline void
			unlock() noexcept {
				_flag.clear(std::memory_order_release);
			}

			inline bool
			try_lock() noexcept {
				return !_flag.test_and_set(std::memory_order_acquire);
			}
	
			inline
			spin_mutex() noexcept = default;
	
			// disallow copy & move operations
			spin_mutex(const spin_mutex&) = delete;
			spin_mutex(spin_mutex&&) = delete;
			spin_mutex& operator=(const spin_mutex&) = delete;
			spin_mutex& operator=(spin_mutex&&) = delete;
	
		private:
			std::atomic_flag _flag = ATOMIC_FLAG_INIT;
		};

		template<class T, std::streamsize W>
		struct Fixed_width {
			Fixed_width(T x): val(x) {}
			friend std::ostream&
			operator<<(std::ostream& out, const Fixed_width& rhs) {
				return out << std::setw(W) << rhs.val;
			}
		private:
			T val;
		};

	}

}
#endif // FACTORY_STDEXT_HH
