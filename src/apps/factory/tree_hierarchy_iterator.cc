#include "tree_hierarchy_iterator.hh"

#include <stdexcept>
#include <unistdx/net/ipv4_addr>
#if !defined(NDEBUG) && defined(FACTORY_DEBUG_TREE_HIERARCHY_ITERATOR)
#include <unistdx/base/log_message>
#endif

template <class Addr>
factory::tree_hierarchy_iterator<Addr>::tree_hierarchy_iterator(
	const ifaddr_type& ifaddr,
	pos_type fanout,
	pos_type offset,
	pos_type stride
):
_ifaddr(ifaddr),
_fanout(fanout),
_offset(offset),
_stride(stride),
_basepos(this->to_virtual_position(ifaddr.position()), fanout),
_parentpos(this->_basepos.layer() - 1, this->_basepos.offset()/fanout, fanout),
_currentpos(this->_basepos) {
	if (this->_fanout == 0) {
		throw std::invalid_argument("fanout=0");
	}
	if (this->_stride == 0) {
		throw std::invalid_argument("stride=0");
	}
	this->next();
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::next() {
	addr_type old_addr;
	do {
		old_addr = this->_address;
		switch (this->_state) {
		case state_type::initial: this->init(); break;
		case state_type::traversing_parent_node:
			this->traverse_parent_node();
			break;
		case state_type::traversing_upper_layers:
			this->traverse_upper_layers();
			break;
		case state_type::traversing_base_layer:
			this->traverse_base_layer();
			break;
		case state_type::end:
			break;
		}
	} while (old_addr == this->_address && this->_state != state_type::end);
	#if !defined(NDEBUG) && defined(FACTORY_DEBUG_TREE_HIERARCHY_ITERATOR)
	sys::log_message(
		"thi",
		"base=_,parent=_,current=_,address=_,state=_",
		_basepos,
		_parentpos,
		_currentpos,
		_address,
		int(_state)
	);
	#endif
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::init() {
	state_type new_state = state_type::traversing_parent_node;
	if (this->_basepos.layer() == 0) {
		new_state = state_type::end;
	}
	this->setstate(new_state);
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::traverse_parent_node() {
	this->set_current_position(this->_parentpos);
	this->setstate(state_type::traversing_upper_layers);
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::traverse_upper_layers() {
	if (this->_state != this->_oldstate) {
		// initialise current position to be the position of the first node
		// in the parent's layer
		this->_currentpos =
			position_type(this->_parentpos.layer(), 0, this->_fanout);
		// update old state
		this->_oldstate = this->_state;
	} else {
		this->advance_upper();
	}
	// skip parent node which has been visited
	// in the first stage
	if (this->_currentpos == this->_parentpos) {
		this->advance_upper();
	}
	this->update_address();
	// update state
	if (this->_currentpos.is_root()) {
		this->setstate(state_type::traversing_base_layer);
	}
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::advance_upper() {
	if (this->_currentpos.is_last()) {
		this->_currentpos.decrement_layer();
		#if !defined(NDEBUG) && defined(FACTORY_DEBUG_TREE_HIERARCHY_ITERATOR)
		sys::log_message("thi", "decrement _", _currentpos);
		#endif
	} else {
		++this->_currentpos;
		#if !defined(NDEBUG) && defined(FACTORY_DEBUG_TREE_HIERARCHY_ITERATOR)
		sys::log_message("thi", "increment");
		#endif
	}
}

template <class Addr>
void
factory::tree_hierarchy_iterator<Addr>::traverse_base_layer() {
	if (this->_state != this->_oldstate) {
		// initialise current position to be the position of the first node
		// in the base node's layer
		this->_currentpos =
			position_type(this->_basepos.layer(), 0, this->_fanout);
		// update old state
		this->_oldstate = this->_state;
	} else {
		++this->_currentpos;
	}
	this->update_address();
	// update state
	if (this->_currentpos == this->_basepos) {
		this->setstate(state_type::end);
	}
}

template class factory::tree_hierarchy_iterator<sys::ipv4_addr>;
