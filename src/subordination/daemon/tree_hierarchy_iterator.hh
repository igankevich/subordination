#ifndef SUBORDINATION_DAEMON_TREE_HIERARCHY_ITERATOR_HH
#define SUBORDINATION_DAEMON_TREE_HIERARCHY_ITERATOR_HH

#include <iterator>
#include <subordination/daemon/position_in_tree.hh>
#include <unistdx/net/interface_address>

namespace sbnd {

    template <class Addr>
    class tree_hierarchy_iterator:
        public std::iterator<std::input_iterator_tag, Addr, void> {

    public:
        typedef Addr addr_type;
        typedef sys::interface_address<Addr> ifaddr_type;
        typedef position_in_tree<Addr> position_type;
        typedef typename addr_type::rep_type pos_type;
        typedef Addr value_type;
        typedef void difference_type;
        typedef value_type* pointer;
        typedef const value_type* const_pointer;
        typedef value_type& reference;
        typedef const value_type& const_reference;
        typedef std::forward_iterator_tag iterator_category;

        enum struct state_type {
            initial = 0,
            traversing_parent_node,
            traversing_upper_layers,
            traversing_base_layer,
            end
        };

    private:
        ifaddr_type _ifaddr;
        pos_type _fanout = 0;
        pos_type _offset = 0;
        pos_type _stride = 1;
        position_type _basepos;
        position_type _parentpos;
        position_type _currentpos;
        addr_type _address{};
        state_type _state = state_type::initial;
        state_type _oldstate = state_type::initial;

    public:

        inline
        tree_hierarchy_iterator() noexcept:
        _state(state_type::end)
        {}

        tree_hierarchy_iterator(
            const ifaddr_type& interface_address,
            pos_type fanout,
            pos_type offset=0,
            pos_type stride=1
        );

        tree_hierarchy_iterator(const tree_hierarchy_iterator&) = default;

        tree_hierarchy_iterator&
        operator=(const tree_hierarchy_iterator&) = default;

        inline bool
        operator==(const tree_hierarchy_iterator& rhs) const noexcept {
            return this->_state == rhs._state;
        }

        inline bool
        operator!=(const tree_hierarchy_iterator& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        inline const_reference
        operator*() const noexcept {
            return this->_address;
        }

        inline const_pointer
        operator->() const noexcept {
            return &this->_address;
        }

        inline tree_hierarchy_iterator&
        operator++() {
            this->next();
            return *this;
        }

        inline tree_hierarchy_iterator
        operator++(int) {
            tree_hierarchy_iterator tmp(*this);
            this->next();
            return tmp;
        }

    private:

        inline pos_type
        to_virtual_position(pos_type rhs) noexcept {
            return rhs / this->_stride - this->_offset;
        }

        inline pos_type
        to_real_position(pos_type rhs) noexcept {
            return this->_offset + rhs*this->_stride;
        }

        inline void
        set_current_position(const position_type& rhs) noexcept {
            this->_currentpos = rhs;
            this->update_address();
        }

        inline void
        update_address() noexcept {
            pos_type vpos = this->_currentpos.to_position_in_address_range();
            pos_type rpos = this->to_real_position(vpos);
            this->_address = *(this->_ifaddr.begin() + rpos);
        }

        void
        next();

        void
        init();

        void
        traverse_parent_node();

        void
        traverse_upper_layers();

        void
        traverse_base_layer();

        /// Advance position for upper layers.
        void
        advance_upper();

        inline void
        setstate(state_type rhs) {
            this->_oldstate = this->_state;
            this->_state = rhs;
        }

    };

}

#endif // vim:filetype=cpp
