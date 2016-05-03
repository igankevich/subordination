#ifndef FACTORY_REFLECTION_HH
#define FACTORY_REFLECTION_HH

#include <factory/type.hh>
#include <factory/kernel.hh>
#include <sys/proc/process.hh>

namespace factory {

	typedef sys::pid_type application_type;

	Instances<Kernel> instances;
	Types<Type<Kernel>> types;

	application_type
	application_id() noexcept {
		return sys::this_process::id();
	}

}

#endif // FACTORY_REFLECTION_HH
