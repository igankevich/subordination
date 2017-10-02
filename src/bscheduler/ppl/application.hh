#ifndef BSCHEDULER_PPL_APPLICATION_HH
#define BSCHEDULER_PPL_APPLICATION_HH

#include <iosfwd>
#include <vector>

#include <unistdx/fs/canonical_path>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/net/pstream>

namespace bsc {

	typedef uint64_t application_type;

	class application {

	public:
		typedef std::string path_type;
		typedef application_type id_type;
		typedef std::vector<std::string> container_type;

	private:
		id_type _id;
		sys::uid_type _uid;
		sys::gid_type _gid;
		container_type _args, _env;
		sys::canonical_path _workdir;
		mutable bool _allowroot = false;

	public:
		explicit
		application(const container_type& args, const container_type& env);

		inline void
		workdir(const sys::canonical_path& rhs) {
			this->_workdir = rhs;
		}

		inline void
		set_credentials(sys::uid_type uid, sys::gid_type gid) noexcept {
			this->_uid = uid;
			this->_gid = gid;
		}

		inline void
		allow_root(bool rhs) const noexcept {
			this->_allowroot = rhs;
		}

		inline id_type
		id() const noexcept {
			return this->_id;
		}

		const std::string&
		filename() const noexcept {
			return this->_args.front();
		}

		int
		execute(const sys::two_way_pipe& pipe) const;

		friend std::ostream&
		operator<<(std::ostream& out, const application& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const application& rhs);

	namespace this_application {

		/// Cluster-wide application ID.
		application_type
		get_id() noexcept;

		sys::fd_type
		get_input_fd() noexcept;

		sys::fd_type
		get_output_fd() noexcept;

	}

}

#endif // vim:filetype=cpp
