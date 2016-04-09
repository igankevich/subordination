#include <sys/dirent.hh>
#include <sys/file.hh>

#include "test.hh"

void
test_dirent_iterator() {
	sys::const_path path = ".";
	sys::directory dir(path);
	/*std::copy(
		dir.begin(), dir.end(),
		std::ostream_iterator<sys::dirent_type>(std::cout, "\n")
	);*/
	std::for_each(
		dir.begin(), dir.end(),
		[&path] (const sys::dirent_type& entry) {
			sys::canonical_path p(path, entry.name());
			sys::file f(p);
			std::cout << f << ' ' << p << '\n';
		}
	);
}

int main() {
	test_dirent_iterator();
	return 0;
}
