#ifndef FACTORY_KERNEL_KERNEL_INSTANCE_REGISTRY_HH
#define FACTORY_KERNEL_KERNEL_INSTANCE_REGISTRY_HH

#include <unordered_map>
#include <mutex>
#include <factory/config.hh>

namespace factory {

	template<class T>
	class kernel_instance_registry {

	public:
		typedef T kernel_type;
		typedef typename kernel_type::id_type id_type;
		typedef std::unordered_map<id_type, kernel_type*> container_type;
		typedef typename container_type::iterator iterator;
		typedef typename container_type::const_iterator const_iterator;
		typedef typename container_type::value_type value_type;

	private:
		container_type _instances;
		mutable id_type _counter = 0;
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
		add(kernel_type* k) {
			if (!k->has_id()) {
				k->id(++this->_counter);
			}
			this->_instances[k->id()] = k;
		}

		inline void
		free_instance(kernel_type* k) {
			this->_instances.erase(k->id());
		}

		template <class X>
		friend std::ostream&
		operator<<(std::ostream& out, const kernel_instance_registry<X>& rhs);

	};

	template <class T>
	std::ostream&
	operator<<(std::ostream& out, const kernel_instance_registry<T>& rhs);

	typedef kernel_instance_registry<FACTORY_KERNEL_TYPE> instance_registry_type;
	typedef std::lock_guard<instance_registry_type> instances_guard;
	extern instance_registry_type instances;

}

#endif // vim:filetype=cpp
