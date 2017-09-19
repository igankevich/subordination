#ifndef EXAMPLES_FACTORY_HIERARCHY_NODE_HH
#define EXAMPLES_FACTORY_HIERARCHY_NODE_HH

#include <iosfwd>

#include <unistdx/net/endpoint>

namespace factory {

	class hierarchy_node {

	public:
		typedef uint32_t weight_type;

	private:
		sys::endpoint _endpoint;
		mutable weight_type _weight = 1;

	public:

		hierarchy_node() = default;

		inline explicit
		hierarchy_node(const sys::endpoint& endpoint):
		_endpoint(endpoint)
		{}

		inline
		hierarchy_node(const sys::endpoint& endpoint, weight_type w):
		_endpoint(endpoint),
		_weight(w)
		{}

		inline void
		reset() {
			this->_endpoint.reset();
			this->_weight = 0;
		}

		inline const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

		inline void
		endpoint(const sys::endpoint& rhs) noexcept {
			this->_endpoint = rhs;
		}

		inline weight_type
		weight() const noexcept {
			return this->_weight;
		}

		inline void
		weight(weight_type rhs) const noexcept {
			this->_weight = rhs;
		}

		inline bool
		operator==(const hierarchy_node& rhs) const noexcept {
			return this->_endpoint == rhs._endpoint;
		}

		inline bool
		operator!=(const hierarchy_node& rhs) const noexcept {
			return !this->operator==(rhs);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const hierarchy_node& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const hierarchy_node& rhs);

	inline bool
	operator==(const hierarchy_node& lhs, const sys::endpoint& rhs) noexcept {
		return lhs.endpoint() == rhs;
	}

	inline bool
	operator==(const sys::endpoint& lhs, const hierarchy_node& rhs) noexcept {
		return lhs == rhs.endpoint();
	}

}

namespace std {

	template<>
	struct hash<factory::hierarchy_node>: public hash<sys::endpoint> {

		typedef size_t result_type;
		typedef factory::hierarchy_node argument_type;

		inline size_t
		operator()(const argument_type& rhs) const noexcept {
			return this->hash<sys::endpoint>::operator()(rhs.endpoint());
		}

	};

}

#endif // vim:filetype=cpp
