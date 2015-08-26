#ifndef FACTORY_UNISTDX_TYPES_HH
#define FACTORY_UNISTDX_TYPES_HH

namespace factory {

	namespace unix {

		typedef ::mode_t mode_type;
		typedef ::pid_t pid_type;
		typedef int fd_type;
		typedef int flag_type;
	
		typedef int stat_type;
		typedef int code_type;
		typedef ::siginfo_t siginfo_type;

		using bits::check;
		using bits::check_if_not;

	}

}

#endif // FACTORY_UNISTDX_TYPES_HH
