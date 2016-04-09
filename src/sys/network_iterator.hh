#ifndef SYSX_NETWORK_ITERATOR_HH
#define SYSX_NETWORK_ITERATOR_HH

#include <iterator>
#include <sys/network_format.hh>

namespace sys {

	template<class Address>
	struct network_iterator:
	public std::iterator<std::random_access_iterator_tag,Address>
	{
		typedef Address addr_type;
		typedef typename addr_type::rep_type rep_type;
		typedef std::iterator<std::random_access_iterator_tag,Address> base_type;
		using typename base_type::difference_type;

		network_iterator() = default;
		network_iterator(const network_iterator&) = default;
		network_iterator(network_iterator&&) = default;
		~network_iterator() = default;
		network_iterator& operator=(const network_iterator&) = default;

		explicit constexpr
		network_iterator(rep_type idx) noexcept:
		_idx(idx),
		_addr(make_addr())
		{}

		explicit constexpr
		network_iterator(const addr_type& addr) noexcept:
		_idx(sys::to_host_format(addr.rep())),
		_addr(addr)
		{}

		constexpr bool
		operator==(const network_iterator& rhs) const noexcept {
			return _idx == rhs._idx;
		}

		constexpr bool
		operator!=(const network_iterator& rhs) const noexcept {
			return !operator==(rhs);
		}

		constexpr const addr_type&
		operator*() const noexcept {
			return _addr;
		}

		constexpr const addr_type*
		operator->() const noexcept {
			return &_addr;
		}

		network_iterator&
		operator++() noexcept {
			++_idx;
			_addr = make_addr();
			return *this;
		}

		network_iterator&
		operator++(int) noexcept {
			network_iterator tmp(*this);
			operator++();
			return tmp;
		}

		constexpr network_iterator
		operator+(difference_type rhs) const noexcept {
			return network_iterator(_idx + rhs);
		}

		constexpr network_iterator
		operator-(difference_type rhs) const noexcept {
			return network_iterator(_idx - rhs);
		}

		constexpr difference_type
		operator-(const network_iterator& rhs) const noexcept {
			return rhs._idx - _idx;
		}

	private:

		addr_type
		make_addr() const noexcept {
			return addr_type(sys::to_network_format(_idx));
		}

		rep_type _idx;
		addr_type _addr;

	};

}

#endif // SYSX_NETWORK_ITERATOR_HH
