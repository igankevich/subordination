#ifndef FACTORY_ID_ALLOCATOR_HH
#define FACTORY_ID_ALLOCATOR_HH

#include <limits>

#include <stdx/interval.hh>

namespace factory {

	template<class I>
	struct Id_allocator {

		typedef I id_type;
		typedef stdx::interval<id_type> interval_type;

		static constexpr const interval_type
		whole_interval(
			std::numeric_limits<id_type>::min(),
			std::numeric_limits<id_type>::max()
		);



	};

}

#endif // FACTORY_ID_ALLOCATOR_HH
