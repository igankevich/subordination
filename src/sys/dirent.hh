#ifndef SYS_DIRENT_HH
#define SYS_DIRENT_HH

#include <dirent.h>
#include <cstring>
#include <system_error>

#include <sys/bits/check.hh>
#include <sys/path.hh>
#include <sys/file.hh>

namespace sys {

	typedef struct ::dirent dirent_type;
	typedef DIR dir_type;
	typedef ::ino_t inode_type;

	struct direntry: public dirent_type {

		constexpr static const char* current_dir = ".";
		constexpr static const char* parent_dir = "..";

		const char*
		name() const noexcept {
			return this->d_name;
		}

		inode_type
		inode() const noexcept {
			return this->d_ino;
		}

		file_type
		type() const noexcept {
			#if defined(__linux__)
			return file_type(DTTOIF(this->d_type));
			#else
			return file_type(0);
			#endif
		}

		bool
		has_type() const noexcept {
			return type() != file_type(0);
		}

		bool
		is_working_dir() const noexcept {
			return !std::strcmp(name(), current_dir);
		}

		bool
		is_parent_dir() const noexcept {
			return !std::strcmp(name(), parent_dir);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const direntry& rhs) {
			return out << rhs.name();
		}

	};

	struct directory {

		enum state {
			goodbit = 0,
			failbit = 1,
			badbit = 2,
			eofbit = 4
		};

		explicit
		directory(const char* path) {
			_dir = ::opendir(path);
			_errc = std::errc(errno);
		}

		directory(directory&& rhs):
		_dir(rhs._dir),
		_errc(rhs._errc)
		{ rhs._dir = nullptr; }

		~directory() {
			if (_dir) {
				bits::check(
					::closedir(_dir),
					__FILE__, __LINE__, __func__
				);
			}
		}

		explicit
		operator bool() const noexcept {
			return !_state;
		}

		bool
		operator!() const noexcept {
			return !operator bool();
		}

		bool
		is_open() const noexcept {
			return _dir != nullptr;
		}

		void
		clear() noexcept {
			_errc = std::errc(0);
			_state = goodbit;
		}

		bool
		good() const noexcept {
			return _state & goodbit;
		}

		bool
		bad() const noexcept {
			return _state & badbit;
		}

		bool
		fail() const noexcept {
			return _state & failbit;
		}

		bool
		eof() const noexcept {
			return _state & eofbit;
		}

		state
		rdstate() const noexcept {
			return _state;
		}

		std::errc
		error_code() const noexcept {
			return _errc;
		}

		directory&
		operator>>(direntry& rhs) {
			dirent_type* result = nullptr;
			int ret = ::readdir_r(_dir, &rhs, &result);
			if (ret == -1) {
				_errc = std::errc(errno);
				_state = state(_state | failbit);
			}
			if (!result) {
				_state = state(_state | eofbit);
			}
			return *this;
		}

	private:

		dir_type* _dir = nullptr;
		std::errc _errc = std::errc(0);
		state _state = goodbit;

	};

	template<class Stream, class Value>
	class basic_istream_iterator: public std::iterator<std::input_iterator_tag, Value> {

		typedef Stream stream_type;
		typedef Value value_type;

	public:

		typedef value_type* pointer;
		typedef const value_type* const_pointer;
		typedef value_type& reference;
		typedef const value_type& const_reference;

		explicit
		basic_istream_iterator(stream_type& rhs) noexcept:
		_stream(&rhs)
		{ next(); }

		basic_istream_iterator() noexcept = default;

		inline
		~basic_istream_iterator() = default;

		basic_istream_iterator(const basic_istream_iterator&) noexcept = default;

		inline basic_istream_iterator&
		operator=(const basic_istream_iterator&) noexcept = default;

		bool
		operator==(const basic_istream_iterator& rhs) const noexcept {
			return this->_stream == rhs._stream;
		}

		bool
		operator!=(const basic_istream_iterator& rhs) const noexcept {
			return !operator==(rhs);
		}

		inline reference
		operator*() noexcept {
			return this->_value;
		}

		const_reference
		operator*() const noexcept {
			return this->_value;
		}

		inline pointer
		operator->() noexcept {
			return &this->_value;
		}

		const_pointer
		operator->() const noexcept {
			return &this->_value;
		}

		inline basic_istream_iterator&
		operator++() {
			this->next(); return *this;
		}

		inline basic_istream_iterator
		operator++(int) {
			basic_istream_iterator tmp(*this); this->next(); return tmp;
		}

	private:

		inline void
		next() {
 			if (not (*_stream >> _value)) {
				_stream = nullptr;
			}
		}

		stream_type* _stream = nullptr;
		value_type _value;

	};

	typedef basic_istream_iterator<directory, direntry> dirent_iterator;

}

#endif // SYS_DIRENT_HH
