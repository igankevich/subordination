#ifndef FACTORY_REFLECTION_HH
#define FACTORY_REFLECTION_HH

#include <factory/type.hh>
#include <factory/kernel.hh>
#include <sys/proc/process.hh>

namespace factory {

	typedef sys::pid_type application_type;

	Instances<Kernel> instances;
	Types types;

	template<class X>
	void
	register_type() {
		types.register_type<X>();
	}

	void
	register_type(const Type& rhs) {
		types.register_type(rhs);
	}

	template<class T, Type::id_type ID=0>
	struct Register_type {

		typedef decltype(ID) id_type;

		Register_type() = default;
		virtual ~Register_type() = default;

		struct Init: public Type {
			Init():
			Type{
				ID,
				[] (sys::packetstream& in) {
					T* kernel = new T;
					kernel->read(in);
					return kernel;
				},
				typeid(T)
			}
			{
				if (ID != 0) {
					types.register_type(*this);
				} else {
					types.register_type<T>();
				}
			}

		};

		static const Init _inittype;

		virtual id_type
		unused() const {
			return _inittype.id();
		}

	};

	template<class T, Type::id_type ID>
	const typename Register_type<T,ID>::Init Register_type<T,ID>::_inittype;

}

#endif // FACTORY_REFLECTION_HH
