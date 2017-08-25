#ifndef FACTORY_PPL_COMPARE_TIME_HH
#define FACTORY_PPL_COMPARE_TIME_HH

namespace factory {

	template<class T>
	struct Compare_time {
		inline bool
		operator()(const T* lhs, const T* rhs) const noexcept {
			return lhs->at() > rhs->at();
		}
	};

}


#endif // FACTORY_PPL_COMPARE_TIME_HH vim:filetype=cpp
