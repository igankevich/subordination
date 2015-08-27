#ifndef FACTORY_STDX_FRONT_POPPER_HH
#define FACTORY_STDX_FRONT_POPPER_HH

#include <queue>

namespace stdx {

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

		value_type&
		operator*() noexcept {
			return front(*container);
		}

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

}

#endif // FACTORY_STDX_FRONT_POPPER_HH
