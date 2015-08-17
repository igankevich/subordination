namespace factory {

	namespace unix {

		typedef ::mode_t mode_type;
		typedef ::pid_t pid_type;
		typedef int fd_type;
		typedef int flag_type;
		typedef struct ::pollfd basic_event;
	
		typedef int stat_type;
		typedef int code_type;
		typedef ::siginfo_t siginfo_type;

		using components::check;
		using components::check_if_not;

	}

}
