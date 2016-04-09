#ifndef SYS_FILE_HH
#define SYS_FILE_HH

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/bits/check.hh>

namespace sys {

	typedef struct ::stat stat_type;
	typedef ::off_t offset_type;
	typedef ::mode_t mode_type;

	enum struct file_type: mode_type {
		socket = S_IFSOCK,
		symbolic_link = S_IFLNK,
		regular = S_IFREG,
		block_device = S_IFBLK,
		directory = S_IFDIR,
		character_device = S_IFCHR,
		fifo = S_IFIFO
	};

	const char*
	to_string(file_type rhs) noexcept {
		switch (rhs) {
			case file_type::socket: return "socket";
			case file_type::symbolic_link: return "symbolic_link";
			case file_type::regular: return "regular";
			case file_type::block_device: return "block_device";
			case file_type::directory: return "directory";
			case file_type::character_device: return "character_device";
			case file_type::fifo: return "fifo";
			default: return "unknown";
		}
	}

	std::ostream&
	operator<<(std::ostream& out, const file_type& rhs) {
		return out << to_string(rhs);
	}

	struct file {

		explicit
		file(const char* filename) {
			bits::check(
				::stat(filename, &_stat),
				__FILE__, __LINE__, __func__
			);
		}

		file_type
		type() const noexcept {
			return file_type(_stat.st_mode & S_IFMT);
		}

		bool
		is_regular() const noexcept {
			return type() == file_type::regular;
		}

		bool
		is_socket() const noexcept {
			return type() == file_type::socket;
		}

		bool
		is_symbolic_link() const noexcept {
			return type() == file_type::symbolic_link;
		}

		bool
		is_block_device() const noexcept {
			return type() == file_type::block_device;
		}

		bool
		is_directory() const noexcept {
			return type() == file_type::directory;
		}

		bool
		is_character_device() const noexcept {
			return type() == file_type::character_device;
		}

		bool
		is_fifo() const noexcept {
			return type() == file_type::fifo;
		}

		offset_type
		size() const noexcept {
			return _stat.st_size;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const file& rhs) {
			return out << rhs.type();
		}

	private:

		stat_type _stat;

	};

}

#endif // SYS_FILE_HH
