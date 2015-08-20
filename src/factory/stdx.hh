#ifndef FACTORY_STDX_HH
#define FACTORY_STDX_HH

#include "bits/stdx.hh"
#include "bits/uint128.hh"

namespace factory {

	namespace stdx {

		template<class T, std::streamsize W>
		struct fixed_width {
			fixed_width(T x): val(x) {}
			friend std::ostream&
			operator<<(std::ostream& out, const fixed_width& rhs) {
				return out << std::setw(W) << rhs.val;
			}
		private:
			T val;
		};

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

		struct ios_guard {
			explicit
			ios_guard(std::ios& s):
				str(s), oldf(str.flags()),
				oldfill(str.fill()) {}
			~ios_guard() {
				str.setf(oldf);
				str.fill(oldfill);
			}
		private:
			std::ios& str;
			std::ios_base::fmtflags oldf;
			std::ios::char_type oldfill;
		};

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

		template<class T1, class T2>
		struct pair: public std::pair<T1,T2> {

			typedef std::pair<T1,T2> base_pair;

			explicit
			pair(T1 x, T2 y):
				base_pair(x, y) {}

			pair(const pair& rhs):
				base_pair(rhs) {}

			pair& operator=(pair&& rhs) {
				base_pair::first = std::move(rhs.first);
				base_pair::second = std::move(rhs.second);
				return *this;
			}
		};

		template<class It1, class It2>
		struct paired_iterator: public std::iterator<
			typename std::iterator_traits<It1>::iterator_category,
			std::tuple<typename std::iterator_traits<It1>::value_type,typename std::iterator_traits<It2>::value_type>,
			std::ptrdiff_t,
			std::tuple<typename std::iterator_traits<It1>::value_type*,typename std::iterator_traits<It2>::value_type*>,
			stdx::pair<typename std::iterator_traits<It1>::reference,typename std::iterator_traits<It2>::reference>
			>
		{
			typedef typename std::iterator_traits<paired_iterator>::reference reference;
			typedef typename std::iterator_traits<paired_iterator>::pointer pointer;
			typedef typename std::iterator_traits<paired_iterator>::difference_type difference_type;
			typedef const reference const_reference;
			typedef const pointer const_pointer;

			typedef typename std::iterator_traits<It1>::value_type&& rvalueref1;
			typedef typename std::iterator_traits<It2>::value_type&& rvalueref2;
			typedef stdx::pair<
				typename std::iterator_traits<It1>::reference,
				typename std::iterator_traits<It2>::reference>
					pair_type;

			inline paired_iterator(It1 x, It2 y): iter1(x), iter2(y) {}
			inline paired_iterator() = default;
			inline ~paired_iterator() = default;
			inline paired_iterator(const paired_iterator&) = default;
			inline paired_iterator& operator=(const paired_iterator&) = default;

			constexpr bool operator==(const paired_iterator& rhs) const { return iter1 == rhs.iter1; }
			constexpr bool operator!=(const paired_iterator& rhs) const { return !this->operator==(rhs); }

			inline const_reference
			operator*() const {
				return pair_type(*iter1,*iter2);
			}

			inline reference
			operator*() {
				return pair_type(*iter1,*iter2);
			}

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
		inline paired_iterator<It1,It2>
		make_paired(It1 it1, It2 it2) {
			return paired_iterator<It1,It2>(it1, it2);
		}

		template<size_t No, class F>
		constexpr bits::Apply_to<No,F>
		apply_to(F&& f) {
			return bits::Apply_to<No,F>(f);
		}

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

		template<class Mutex>
		struct simple_lock {
			
			typedef Mutex mutex_type;

			inline
			simple_lock(mutex_type& m) noexcept:
			mtx(m)
			{
				lock();
			}

			inline
			~simple_lock() noexcept {
				unlock();
			}

			inline void
			lock() noexcept {
				mtx.lock();
			}

			inline void
			unlock() noexcept {
				mtx.unlock();
			}

			// disallow copy & move operations
			simple_lock() = delete;
			simple_lock(const simple_lock&) = delete;
			simple_lock(simple_lock&&) = delete;
			simple_lock& operator=(const simple_lock&) = delete;
			simple_lock& operator=(simple_lock&&) = delete;

		private:
			Mutex& mtx;
		};

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

		template<class C>
		inline auto
		front(C& container) noexcept
		-> decltype(container.front())
		{
			return container.front();
		}

		template<class C>
		inline auto
		front(const C& container) noexcept
		-> decltype(container.front())
		{
			return container.front();
		}

		template<class T, class Cont, class Comp>
		inline auto
		front(std::priority_queue<T,Cont,Comp>& container) noexcept
		-> decltype(container.top())
		{
			return container.top();
		}

		template<class T, class Cont, class Comp>
		inline auto
		front(const std::priority_queue<T,Cont,Comp>& container) noexcept
		-> decltype(container.top())
		{
			return container.top();
		}

		template<class C>
		inline auto
		pop(C& container) noexcept
		-> decltype(container.pop())
		{
			return container.pop();
		}

		template<class T, class Alloc>
		inline auto
		pop(std::deque<T,Alloc>& container) noexcept
		-> decltype(container.pop_front())
		{
			return container.pop_front();
		}

		template<class T, class Alloc>
		inline auto
		pop(std::list<T,Alloc>& container) noexcept
		-> decltype(container.pop_front())
		{
			return container.pop_front();
		}

		template<class Container>
		struct front_pop_iterator:
		    public std::iterator<std::input_iterator_tag,
				typename Container::value_type>
		{

			typedef Container container_type;
			typedef typename Container::value_type value_type;

			explicit constexpr
			front_pop_iterator(container_type& x) noexcept:
			container(&x) {}

			constexpr
			front_pop_iterator() noexcept = default;

			constexpr
			front_pop_iterator(const front_pop_iterator& rhs) noexcept = default;

			inline bool
			operator==(const front_pop_iterator&) const noexcept {
				return container->empty();
			}

			inline bool
			operator!=(const front_pop_iterator& rhs) const noexcept {
				return !operator==(rhs);
			}

			const value_type&
			operator*() const noexcept {
				return front(*container);
			}

//			value_type&
//			operator*() noexcept {
//				return front(*container);
//			}

			value_type*
			operator->() noexcept {
				return &front(*container);
			}

			const value_type*
			operator->() const noexcept {
				return &front(*container);
			}

			inline front_pop_iterator&
			operator++() noexcept {
				pop(*container);
				return *this;
			}

			inline front_pop_iterator&
			operator++(int) noexcept {
				throw std::logic_error("can't post increment stdx::front_pop_iterator");
				return *this;
			}

		private:
			container_type* container = nullptr;
		};

		template<class C>
		inline front_pop_iterator<C>
		front_popper(C& cont) noexcept {
			return front_pop_iterator<C>(cont);
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

		template<class C>
		inline void
		push(C& container, const typename C::value_type& rhs) {
			return container.push_back(rhs);
		}

		template<class C>
		inline void
		push(C& container, const typename C::value_type&& rhs) {
			return container.push_back(std::move(rhs));
		}

		template<class T, class Cont>
		inline void
		push(std::queue<T,Cont>& container, 
			const typename std::queue<T,Cont>::value_type& rhs) {
			return container.push(rhs);
		}

		template<class T, class Cont>
		inline void
		push(std::queue<T,Cont>& container, 
			const typename std::queue<T,Cont>::value_type&& rhs) {
			return container.push(std::move(rhs));
		}

		template<class T, class Cont, class Comp>
		inline void
		push(std::priority_queue<T,Cont,Comp>& container, 
			const typename std::priority_queue<T,Cont,Comp>::value_type& rhs) {
			return container.push(rhs);
		}

		template<class T, class Cont, class Comp>
		inline void
		push(std::priority_queue<T,Cont,Comp>& container, 
			const typename std::priority_queue<T,Cont,Comp>::value_type&& rhs) {
			return container.push(std::move(rhs));
		}

		template <class Container>
		struct back_insert_iterator:
		public std::iterator<std::output_iterator_tag,void,void,void,void> {
		
			typedef Container container_type;
			typedef typename Container::value_type value_type;

			explicit inline
			back_insert_iterator(Container& x) noexcept: container(x) {}

			inline back_insert_iterator&
			operator=(const value_type& rhs) {
				push(container, rhs);
				return *this;
			}

			inline back_insert_iterator&
			operator=(const value_type&& rhs) {
				push(container, std::move(rhs));
				return *this;
			}

			inline back_insert_iterator&
			operator*() noexcept {
				return *this;
			}

			inline back_insert_iterator&
			operator++() noexcept {
				return *this;
			}

			inline back_insert_iterator
			operator++(int) noexcept {
				return *this;
			}

		private:
			Container& container;
		};

		template<class Container>
		inline back_insert_iterator<Container>
		back_inserter(Container& rhs) {
			return back_insert_iterator<Container>(rhs);
		}

		template<class It, size_t No, class Field>
		struct field_iterator: public std::iterator<
			typename std::iterator_traits<It>::iterator_category,
			Field,
			typename std::iterator_traits<It>::difference_type,
			Field*,
			Field&
		>
		{

			typedef Field& reference;
			typedef Field* pointer;
			typedef typename std::iterator_traits<It>::difference_type difference_type;
			typedef const Field& const_reference;
			typedef const Field* const_pointer;

			explicit inline
			field_iterator(It rhs): iter(rhs) {}

			inline field_iterator() = default;
			inline ~field_iterator() = default;
			inline field_iterator(const field_iterator&) = default;
			inline field_iterator& operator=(const field_iterator&) = default;

			constexpr bool operator==(const field_iterator& rhs) const { return iter == rhs.iter; }
			constexpr bool operator!=(const field_iterator& rhs) const { return !this->operator==(rhs); }

			inline const_reference
			operator*() const {
				return std::get<No>(*iter);
			}

//			inline reference
//			operator*() {
//				return std::get<No>(*iter);
//			}

			inline const_pointer
			operator->() const {
				return std::get<No>(iter.operator->());
			}

//			inline pointer
//			operator->() {
//				return std::get<No>(iter.operator->());
//			}

			inline field_iterator&
			operator++() {
				++iter;
				return *this;
			}

			inline field_iterator operator++(int) { field_iterator tmp(*this); ++iter; return tmp; }
			inline difference_type operator-(const field_iterator& rhs) const {
				return std::distance(rhs.iter, iter);
			}
			inline field_iterator operator+(difference_type rhs) const {
				return field_iterator(std::advance(iter, rhs));
			}

		private:
			It iter;
		};

		template<size_t No, class It>
		inline auto
		field_iter(It it)
		-> field_iterator<It, No,
		typename std::remove_const<typename std::remove_reference<decltype(std::get<No>(*it))>::type>::type>
		{
			return field_iterator<It, No,
			typename std::remove_const<typename std::remove_reference<decltype(std::get<No>(*it))>::type>::type
			>(it);
		}

	}

}
#endif // FACTORY_STDX_HH
