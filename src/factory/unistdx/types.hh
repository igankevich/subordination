namespace factory {

	typedef ::pid_t pid_type;
	typedef ::mode_t mode_type;
	typedef int fd_type;
	typedef int flag_type;

	namespace unix {

		typedef ::mode_t mode_type;
		typedef int flag_type;

		using components::check;
		using components::check_if_not;

	}

}
