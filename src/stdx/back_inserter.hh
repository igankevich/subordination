#ifndef FACTORY_STDX_BACK_INSERTER_HH
#define FACTORY_STDX_BACK_INSERTER_HH

#include <queue>

namespace stdx {

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

}

#endif // FACTORY_STDX_BACK_INSERTER_HH
