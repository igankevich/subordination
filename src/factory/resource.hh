// TODO: delete this eventually
#include <dirent.h>
namespace factory {
	
	typedef std::string Resource;

	struct Resources {

		Resources() {
			add_files_from_current_directory_as_resources();
		}

		const Endpoint* lookup(const Resource& res) const {
			auto result = _resources.find(res);
			return result == _resources.end() ? nullptr : result->second; 
		}

		friend std::ostream& operator<<(std::ostream& out, const Resources& rhs) {
			for (auto it=rhs._resources.cbegin(); it!=rhs._resources.cend(); ++it) {
				out << "/resource/" << it->first << " = " << *it->second << std::endl;
			}
			return out;
		}

		static Resources& resources() {
			static Resources x;
			return x;
		}

		const std::unordered_map<Resource, Endpoint*>& map() const {
			return _resources;
		}

	private:

		// TODO: temporary solution
		void add_files_from_current_directory_as_resources() {
			DIR* directory = ::opendir(".");
			if (directory) {
				struct dirent* entry;
				while ((entry = ::readdir(directory)) != NULL) {
					std::string filename = entry->d_name;
					if (filename != "." && filename != "..") {
						add_files_from(filename);
					}
				}
				::closedir(directory);
			}
		}
		void add_files_from(std::string folder) {
			std::string prefix =  (folder + "/");
//			std::clog << "Opening " << folder << std::endl;
			DIR* directory = ::opendir(folder.c_str());
			if (directory) {
				struct dirent* entry;
				while ((entry = ::readdir(directory)) != NULL) {
					std::string filename = entry->d_name;
					if (filename != "." && filename != "..") {
						Endpoint* endp = new Endpoint;
						std::stringstream tmp(folder);
						tmp >> *endp;
						std::string key = prefix + filename;
						_resources[key.substr(key.find('/') + 1)] = endp;
						add_files_from(prefix + filename);
					}
				}
				::closedir(directory);
			}
		}

		std::unordered_map<Resource, Endpoint*> _resources;
	};

	struct Kernels {

		const Endpoint* lookup(Id i) const {
			auto result = _kernels.find(i);
			return result == _kernels.end() ? nullptr : result->second; 
		}

		void register_kernel(Id i, Endpoint* endp) {
			_kernels[i] = endp;
		}

		friend std::ostream& operator<<(std::ostream& out, const Kernels& rhs) {
			for (auto it=rhs._kernels.cbegin(); it!=rhs._kernels.cend(); ++it) {
				out << it->first << " -> " << *it->second << std::endl;
			}
			return out;
		}

		static Kernels& kernels() {
			static Kernels x;
			return x;
		}

		const std::unordered_map<Id, Endpoint*>& map() const {
			return _kernels;
		}

	private:

		std::unordered_map<Id, Endpoint*> _kernels;
	};

}
