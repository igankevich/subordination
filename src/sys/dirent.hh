#ifndef SYS_DIRENT_HH
#define SYS_DIRENT_HH

#include <dirent.h>

#include <sys/bits/check.hh>
#include <sys/path.hh>

namespace sys {

	typedef struct ::dirent basic_dirent_type;
	typedef DIR dir_type;

	struct dirent_type: public basic_dirent_type {

		const char*
		name() const noexcept {
			return this->d_name;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const dirent_type& rhs) {
			return out << rhs.name();
		}

	};

	struct dirent_iterator: public std::iterator<std::input_iterator_tag, dirent_type> {

		typedef dirent_type* pointer;
		typedef const dirent_type* const_pointer;
		typedef dirent_type& reference;
		typedef const dirent_type& const_reference;

		explicit
		dirent_iterator(dir_type* rhs) noexcept: _dir(rhs) {
			if (_dir) {
				next();
			}
		}

		dirent_iterator() noexcept = default;

		inline
		~dirent_iterator() = default;

		dirent_iterator(const dirent_iterator&) noexcept = default;

		inline dirent_iterator&
		operator=(const dirent_iterator&) noexcept = default;

		bool
		operator==(const dirent_iterator& rhs) const noexcept {
			return this->_dir == rhs._dir;
		}

		bool
		operator!=(const dirent_iterator& rhs) const noexcept {
			return !operator==(rhs);
		}

		inline reference
		operator*() noexcept {
			return this->_entry;
		}

		const_reference
		operator*() const noexcept {
			return this->_entry;
		}

		inline pointer
		operator->() noexcept {
			return &this->_entry;
		}

		const_pointer
		operator->() const noexcept {
			return &this->_entry;
		}

		inline dirent_iterator&
		operator++() {
			this->next(); return *this;
		}

		inline dirent_iterator
		operator++(int) {
			dirent_iterator tmp(*this); this->next(); return tmp;
		}

	private:

		inline void
		next() {
			basic_dirent_type* result = nullptr;
			bits::check(
				::readdir_r(_dir, &_entry, &result),
				__FILE__, __LINE__, __func__
			);
			if (not result) {
				_dir = nullptr;
			}
		}

		dir_type* _dir = nullptr;
		dirent_type _entry;
	};

	struct directory {

		typedef dirent_iterator iterator;
		typedef dirent_type value_type;
		typedef size_t size_type;

		explicit
		directory(const char* path) {
			_dir = bits::check<dir_type*>(
				::opendir(path),
				static_cast<dir_type*>(0),
				__FILE__, __LINE__, __func__
			);
		}

		directory(directory&& rhs):
		_dir(rhs._dir)
		{ rhs._dir = nullptr; }

		~directory() {
			if (_dir) {
				bits::check(
					::closedir(_dir),
					__FILE__, __LINE__, __func__
				);
			}
		}

		iterator
		begin() noexcept {
			return iterator(this->_dir);
		}

		iterator
		begin() const noexcept {
			return iterator(this->_dir);
		}

		static iterator
		end() noexcept {
			return iterator();
		}

		bool
		empty() const noexcept {
			return this->begin() == this->end();
		}

		size_type
		size() const noexcept {
			return std::distance(this->begin(), this->end());
		}

	private:

		dir_type* _dir = nullptr;

	};

}

#endif // SYS_DIRENT_HH
