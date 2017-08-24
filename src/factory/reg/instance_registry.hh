#ifndef FACTORY_INSTANCE_REGISTRY_HH
#define FACTORY_INSTANCE_REGISTRY_HH

#include <unordered_map>
#include <mutex>

namespace factory {

	template<class T>
	class Instance_registry {

	public:
		typedef T kernel_type;
		typedef typename kernel_type::id_type id_type;
		typedef std::unordered_map<id_type, kernel_type*> container_type;
		typedef typename container_type::iterator iterator;
		typedef typename container_type::const_iterator const_iterator;

	private:
		container_type _instances;
		mutable std::mutex _mutex;

	public:

		inline void
		lock() {
			this->_mutex.lock();
		}

		inline void
		unlock() {
			this->_mutex.unlock();
		}

		inline const_iterator
		begin() const noexcept {
			return this->_instances.begin();
		}

		inline const_iterator
		end() const noexcept {
			return this->_instances.end();
		}

		inline const_iterator
		find(id_type id) {
			return this->_instances.find(id);
		}

		inline void
		register_instance(kernel_type* k) {
			this->_instances[k->id()] = k;
		}

		inline void
		free_instance(kernel_type* k) {
			this->_instances.erase(k->id());
		}

		template <class X>
		friend std::ostream&
		operator<<(std::ostream& out, const Instance_registry<X>& rhs);

	};

	template <class T>
	std::ostream&
	operator<<(std::ostream& out, const Instance_registry<T>& rhs);

}

#endif // FACTORY_INSTANCE_REGISTRY_HH vim:filetype=cpp
