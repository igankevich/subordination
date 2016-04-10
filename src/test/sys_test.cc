#include <sys/dirent.hh>
#include <sys/file.hh>

#include "test.hh"

void
test_dirent_iterator() {
	sys::const_path path = ".";
//	sys::directory dir(path);
	/*std::copy(
		dir.begin(), dir.end(),
		std::ostream_iterator<sys::direntry>(std::cout, "\n")
	);*/
	/*std::for_each(
		dir.begin(), dir.end(),
		[&path] (const sys::direntry& entry) {
			sys::path p(path, entry.name());
			sys::file_stat f(p);
			std::cout << f << ' ' << p << '\n';
		}
	);*/
	sys::dirtree tree(path);
	std::copy(
		sys::dirtree_iterator<sys::file>(tree),
		sys::dirtree_iterator<sys::file>(),
		std::ostream_iterator<sys::file>(std::cout, "\n")
	);
}

int main() {
	test_dirent_iterator();
	return 0;
}
