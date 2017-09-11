#ifndef FACTORY_PPL_SHARED_FILDES_HH
#define FACTORY_PPL_SHARED_FILDES_HH

namespace factory {

	enum Shared_fildes: sys::fd_type {
		In  = 100,
		Out = 101
	};

}

#endif // vim:filetype=cpp
