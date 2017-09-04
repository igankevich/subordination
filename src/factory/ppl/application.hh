#ifndef FACTORY_PPL_APPLICATION_HH
#define FACTORY_PPL_APPLICATION_HH

#include <unistdx/ipc/execute>
#include <iosfwd>

namespace factory {

	typedef uint64_t application_type;

	class Application {

	public:
		typedef std::string path_type;
		typedef application_type id_type;

	private:
		path_type _execpath;
		id_type _id;

	public:
		explicit
		Application(const path_type& exec);

		inline id_type
		id() const noexcept {
			return this->_id;
		}

		int
		execute() const;

		friend std::ostream&
		operator<<(std::ostream& out, const Application& rhs);

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
