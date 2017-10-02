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
		enum class process_role_type {
			master,
			slave
		};

	private:
		id_type _id = 0;
		sys::uid_type _uid = -1;
		sys::gid_type _gid = -1;
		container_type _args, _env;
		sys::canonical_path _workdir;
		mutable bool _allowroot = false;
		mutable process_role_type _processrole = process_role_type::master;

		static_assert(sizeof(_uid) <= sizeof(uint32_t), "bad uid_type");
		static_assert(sizeof(_gid) <= sizeof(uint32_t), "bad gid_type");

	public:
		application(const container_type& args, const container_type& env);

		application() = default;

		application(application&& rhs) = default;

		application(const application& rhs) = default;

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

		inline void
		make_master() const noexcept {
			this->_processrole = process_role_type::master;
		}

		inline void
		make_slave() const noexcept {
			this->_processrole = process_role_type::slave;
		}

		inline bool
		is_master() const noexcept {
			return this->_processrole == process_role_type::master;
		}

		inline bool
		is_slave() const noexcept {
			return this->_processrole == process_role_type::slave;
		}

		int
		execute(const sys::two_way_pipe& pipe) const;

		void
		write(sys::pstream& out) const;

		void
		read(sys::pstream& in);

		friend std::ostream&
		operator<<(std::ostream& out, const application& rhs);

		friend void
		swap(application& lhs, application& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const application& rhs);

	inline sys::pstream&
	operator<<(sys::pstream& out, const application& rhs) {
		rhs.write(out);
		return out;
	}

	inline sys::pstream&
	operator>>(sys::pstream& in, application& rhs) {
		rhs.read(in);
		return in;
	}

	void
	swap(application& lhs, application& rhs);

	namespace this_application {

		/// Cluster-wide application ID.
		application_type
		get_id() noexcept;

		sys::fd_type
		get_input_fd() noexcept;

		sys::fd_type
		get_output_fd() noexcept;

		bool
		is_master() noexcept;

		bool
		is_slave() noexcept;

	}

}

#endif // vim:filetype=cpp
