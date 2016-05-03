#ifndef FACTORY_REFLECTION_HH
#define FACTORY_REFLECTION_HH

#include <factory/type.hh>
#include <factory/kernel.hh>

namespace factory {

	Instances<Kernel> instances;
	Types<Type<Kernel>> types;

}

#endif // FACTORY_REFLECTION_HH
