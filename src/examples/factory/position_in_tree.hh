#ifndef APPS_FACTORY_POSITION_IN_TREE_HH
#define APPS_FACTORY_POSITION_IN_TREE_HH

#include <cassert>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <tuple>

namespace asc {

	template <class Addr>
	class position_in_tree {

	public:
		typedef Addr addr_type;
		typedef typename addr_type::rep_type pos_type;

	private:
		pos_type _layer = 0;
		pos_type _offset = 0;
		pos_type _fanout = 0;

	public:
		inline
		position_in_tree(
			pos_type layer,
			pos_type offset,
			pos_type fanout
		) noexcept:
		_layer(layer),
		_offset(offset),
		_fanout(fanout)
		{}

		position_in_tree(pos_type linear_pos, pos_type fanout) noexcept;

		position_in_tree() = default;

		~position_in_tree() = default;

		position_in_tree(const position_in_tree&) = default;

		position_in_tree(position_in_tree&&) = default;

		position_in_tree&
		operator=(const position_in_tree&) = default;

		position_in_tree&
		operator=(position_in_tree&&) = default;

		inline bool
		operator==(const position_in_tree& rhs) const noexcept {
			return this->_layer == rhs._layer &&
			       this->_offset == rhs._offset &&
			       this->_fanout == rhs._fanout;
		}

		inline bool
		operator!=(const position_in_tree& rhs) const noexcept {
			return !this->operator==(rhs);
		}

		/// N.B. Fanout is not used in this method.
		inline bool
		operator<(const position_in_tree& rhs) const noexcept {
			assert(this->_fanout == rhs._fanout);
			return std::tie(this->_layer, this->_offset) <
			       std::tie(rhs._layer, rhs._offset);
		}

		position_in_tree&
		operator++() noexcept;

		position_in_tree&
		operator--() noexcept;

		inline bool
		is_root() const noexcept {
			return this->_layer == 0 && this->_offset == 0;
		}

		inline bool
		is_last() const noexcept {
			return this->_offset == this->num_nodes() - 1;
		}

		/*
		inline bool
		is_previous_to(const position_in_tree& rhs) const noexcept {
			return (rhs._offset == 0 &&
			        this->_layer == rhs._layer - 1 &&
			        this->_offset == this->num_nodes() - 1)
			       || (rhs._offset != 0 &&
			           this->_layer == rhs._layer &&
			           this->_offset == rhs._offset - 1);
		}
		*/

		inline pos_type
		layer() const noexcept {
			return this->_layer;
		}

		inline pos_type
		offset() const noexcept {
			return this->_offset;
		}

		inline pos_type
		fanout() const noexcept {
			return this->_fanout;
		}

		inline void
		decrement_layer() noexcept {
			if (this->_layer > 0) {
				--this->_layer;
				this->_offset = 0;
			}
		}

		pos_type
		num_nodes() const noexcept;

		pos_type
		to_position_in_address_range() const noexcept;

		template <class X>
		friend std::ostream&
		operator<<(std::ostream& out, const position_in_tree<X>& rhs);

	};

	template <class Addr>
	std::ostream&
	operator<<(std::ostream& out, const position_in_tree<Addr>& rhs);

}

namespace std {

	template<class Addr>
	struct hash<asc::position_in_tree<Addr>>:
		public std::hash<typename asc::position_in_tree<Addr>::pos_type> {

		typedef size_t result_type;
		typedef asc::position_in_tree<Addr> argument_type;
		typedef typename asc::position_in_tree<Addr>::pos_type pos_type;

		inline result_type
		operator()(const argument_type& rhs) const noexcept {
			typedef std::hash<pos_type> tp;
			return
			    tp::operator()(rhs.layer()) ^
			    tp::operator()(rhs.offset());

		}

	};

}

#endif // vim:filetype=cpp
