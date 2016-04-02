#ifndef STDX_ITERATOR_HH
#define STDX_ITERATOR_HH

#include <iterator>
#include <ostream>
#include <queue>
#include <utility>
#include <tuple>

namespace stdx {

	template <class T, class Delim=const char*, class Ch=char, class Tr=std::char_traits<Ch>>
	struct intersperse_iterator:
		public std::iterator<std::output_iterator_tag, void, void, void, void>
	{
		typedef Ch char_type;
		typedef Tr traits_type;
		typedef std::basic_ostream<Ch,Tr> ostream_type;
		typedef Delim delim_type;

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

	/*
	template<class T, class Alloc>
	inline auto
	pop(std::list<T,Alloc>& container) noexcept
	-> decltype(container.pop_front())
	{
		return container.pop_front();
	}
	*/

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

//		value_type&
//		operator*() noexcept {
//			return front(*container);
//		}

//		value_type*
//		operator->() noexcept {
//			return &front(*container);
//		}

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

	template<class C>
	inline front_pop_iterator<C>
	front_popper_end(C& cont) noexcept {
		return front_pop_iterator<C>();
	}

	/// A pair with ``move'' assignment operator.
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
		stdx::pair<typename std::iterator_traits<It1>::value_type,typename std::iterator_traits<It2>::value_type>,
		std::ptrdiff_t,
		stdx::pair<typename std::iterator_traits<It1>::value_type*,typename std::iterator_traits<It2>::value_type*>,
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

		inline const_pointer operator->() const { return std::make_pair(iter1.operator->(),iter2.operator->()); }
		inline pointer operator->() { return std::make_pair(iter1.operator->(),iter2.operator->()); }
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
	struct Apply_to {

		constexpr explicit
		Apply_to(F&& f):
		func(std::forward<F>(f))
		{}

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
		return Apply_to<No,F>(std::forward<F>(f));
	}

}

namespace std {

	namespace bits {

		template<class Pointer>
		typename std::pointer_traits<Pointer>::difference_type
		cstring_arr_size(Pointer first) {
			typename std::pointer_traits<Pointer>::difference_type n=0;
			if (first) {
				while (*first) {
					++n;
					++first;
				}
			}
			return n;
		}

	}

	template<class T>
	T*
	begin(T* rhs) noexcept {
		return rhs;
	}

	template<class T>
	const T*
	begin(const T* rhs) noexcept {
		return rhs;
	}

	template<class T>
	T*
	end(T* rhs) noexcept {
		return rhs + bits::cstring_arr_size(rhs);
	}

	template<class T>
	const T*
	end(const T* rhs) noexcept {
		return rhs + bits::cstring_arr_size(rhs);
	}

}

#endif // STDX_ITERATOR_HH
