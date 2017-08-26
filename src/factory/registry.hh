#ifndef FACTORY_REGISTRY_HH
#define FACTORY_REGISTRY_HH

#include <factory/config.hh>
#include <factory/reg/instance_registry.hh>
#include <factory/reg/type_registry.hh>
#include <unistdx/ipc/process>

namespace factory {

	typedef sys::pid_type application_type;
	typedef Instance_registry<FACTORY_KERNEL_TYPE> instance_registry_type;
	typedef std::lock_guard<instance_registry_type> instances_guard;

	extern instance_registry_type instances;
	extern Types types;

	template<class X>
	inline void
	register_type() {
		types.register_type<X>();
	}

	inline void
	register_type(const Type& rhs) {
		types.register_type(rhs);
	}

}

#endif // vim:filetype=cpp
