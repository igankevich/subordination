#ifndef FACTORY_REFLECTION_HH
#define FACTORY_REFLECTION_HH

#include <factory/type.hh>
#include <factory/kernel.hh>
#include <sys/proc/process.hh>

namespace factory {

	typedef sys::pid_type application_type;

	Instances<Kernel> instances;
	Types<Type<Kernel>> types;
	application_type appid = sys::this_process::id();

	application_type
	application_id() noexcept {
		return appid;
	}

	void
	set_application_id(application_type rhs) noexcept {
		appid = rhs;
	}

}

#endif // FACTORY_REFLECTION_HH
