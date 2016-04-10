#include <sys/dirent.hh>
#include <sys/file.hh>

#include "test.hh"

#include <queue>

template<class Path, class Func>
void
for_each_file(Path starting_point, Func func) {
	std::queue<sys::path> dirs;
	dirs.emplace(starting_point);
	while (!dirs.empty()) {
		sys::path path(std::move(dirs.front()));
		sys::directory dir(path);
		dirs.pop();
		std::for_each(
			dir.begin(), dir.end(),
			[&path,&func,&dirs] (const sys::dirent_type& entry) {
				if (!entry.is_working_dir() && !entry.is_parent_dir()) {
					sys::path p(path, entry.name());
					sys::file f(p);
					if (f.is_directory()) {
						dirs.emplace(p);
					}
					func(f, p);
				}
			}
		);
	}
}

void
test_dirent_iterator() {
	sys::const_path path = ".";
//	sys::directory dir(path);
	/*std::copy(
		dir.begin(), dir.end(),
		std::ostream_iterator<sys::dirent_type>(std::cout, "\n")
	);*/
	/*std::for_each(
		dir.begin(), dir.end(),
		[&path] (const sys::dirent_type& entry) {
			sys::path p(path, entry.name());
			sys::file f(p);
			std::cout << f << ' ' << p << '\n';
		}
	);*/
	for_each_file(
		path,
		[] (const sys::file& f, const sys::path& p) {
			std::cout << f << ' ' << p << '\n';
		}
	);
}

int main() {
	test_dirent_iterator();
	return 0;
}
