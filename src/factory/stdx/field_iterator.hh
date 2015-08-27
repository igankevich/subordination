#ifndef FACTORY_STDX_FIELD_ITERATOR_HH
#define FACTORY_STDX_FIELD_ITERATOR_HH

#include <iterator>
#include <tuple>

namespace stdx {

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

		//inline reference
		//operator*() {
		//	return std::get<No>(*iter);
		//}

		inline const_pointer
		operator->() const {
			return std::get<No>(iter.operator->());
		}

		//inline pointer
		//operator->() {
		//	return std::get<No>(iter.operator->());
		//}

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
	typename std::decay<decltype(std::get<No>(*it))>::type>
	{
		return field_iterator<It, No,
		typename std::decay<decltype(std::get<No>(*it))>::type
		>(it);
	}

}

#endif // FACTORY_STDX_FIELD_ITERATOR_HH
