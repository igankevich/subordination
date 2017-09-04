#ifndef FACTORY_PPL_APPLICATION_HH
#define FACTORY_PPL_APPLICATION_HH

#include <unistdx/ipc/execute>
#include <iosfwd>

namespace factory {

	typedef uint64_t application_type;

	struct Application {

		typedef std::string path_type;

		static const application_type ROOT = 0;

		Application() = default;

		inline explicit
		Application(const path_type& exec):
		_execpath(exec)
		{}

		inline int
		execute() const {
			return sys::this_process::execute(this->_execpath);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Application& rhs);

	private:

		path_type _execpath;

	};

	std::ostream&
	operator<<(std::ostream& out, const Application& rhs);

	namespace this_application {

		/// Cluster-wide application ID.
		application_type
		get_id() noexcept;

	}

}

#endif // vim:filetype=cpp
