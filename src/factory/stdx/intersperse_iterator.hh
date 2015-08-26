#ifndef FACTORY_STDX_INTERSPERSE_ITERATOR_HH
#define FACTORY_STDX_INTERSPERSE_ITERATOR_HH

#include <iterator>
#include <ostream>

namespace factory {

	namespace stdx {

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

	}

}

#endif // FACTORY_STDX_INTERSPERSE_ITERATOR_HH
