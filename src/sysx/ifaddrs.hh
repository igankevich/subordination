#ifndef SYSX_IFADDRS_HH
#define SYSX_IFADDRS_HH

namespace sysx {

	typedef struct ::ifaddrs ifaddrs_type;

	struct ifaddrs_iterator: public std::iterator<std::input_iterator_tag, ifaddrs_type> {

		typedef ifaddrs_type* pointer;
		typedef const ifaddrs_type* const_pointer;
		typedef ifaddrs_type& reference;
		typedef const ifaddrs_type& const_reference;

		explicit constexpr
		ifaddrs_iterator(pointer rhs) noexcept: _ifa(rhs) {}

		constexpr
		ifaddrs_iterator() noexcept = default;

		inline
		~ifaddrs_iterator() = default;

		constexpr
		ifaddrs_iterator(const ifaddrs_iterator&) noexcept = default;

		inline ifaddrs_iterator&
		operator=(const ifaddrs_iterator&) noexcept = default;

		constexpr bool
		operator==(const ifaddrs_iterator& rhs) const noexcept {
			return this->_ifa == rhs._ifa;
		}

		constexpr bool
		operator!=(const ifaddrs_iterator& rhs) const noexcept {
			return !operator==(rhs);
		}

		inline reference
		operator*() noexcept {
			return *this->_ifa;
		}

		constexpr const_reference
		operator*() const noexcept {
			return *this->_ifa;
		}

		inline pointer
		operator->() noexcept {
			return this->_ifa;
		}

		constexpr const_pointer
		operator->() const noexcept {
			return this->_ifa;
		}

		inline ifaddrs_iterator&
		operator++() noexcept {
			this->next(); return *this;
		}

		inline ifaddrs_iterator
		operator++(int) noexcept {
			ifaddrs_iterator tmp(*this); this->next(); return tmp;
		}

	private:

		inline void
		next() noexcept {
			this->_ifa = this->_ifa->ifa_next;
		}

		pointer _ifa = nullptr;
	};

	struct ifaddrs {

		typedef ifaddrs_type value_type;
		typedef ifaddrs_iterator iterator;
		typedef std::size_t size_type;

		ifaddrs() {
			bits::check(::getifaddrs(&this->_addrs),
				__FILE__, __LINE__, __func__);
		}
		~ifaddrs() noexcept {
			if (this->_addrs) {
				::freeifaddrs(this->_addrs);
			}
		}

		iterator
		begin() noexcept {
			return iterator(this->_addrs);
		}

		iterator
		begin() const noexcept {
			return iterator(this->_addrs);
		}

		static constexpr iterator
		end() noexcept {
			return iterator();
		}

		bool
		empty() const noexcept {
			return this->begin() == this->end();
		}

		size_type
		size() const noexcept {
			return std::distance(this->begin(), this->end());
		}

	private:

		ifaddrs_type* _addrs = nullptr;

	};

}

#endif // SYSX_IFADDRS_HH
