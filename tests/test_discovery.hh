#include <unistd.h>
#include <lxc/lxccontainer.h>

void lxc_check(const char* func, struct lxc_container* c, bool ret) {
	if (!ret) {
		std::stringstream msg;
		msg << func << " returned false. " << c->error_string;
		throw std::runtime_error(msg.str());
	}
}


struct Tmp_file {

	Tmp_file(const char* container_name): _name(tmp_filename()) {
//		mkdir(_name.c_str(), 0755);
//		_name += "/";
//		_name += container_name;
//		mkdir(_name.c_str(), 0755);
//		_name += "/config";
		std::ofstream tmp(_name);
	}
	~Tmp_file() { std::remove(_name.c_str()); }
	const char* name() const { return _name.c_str(); }

private:

	std::string tmp_filename() {
		std::stringstream name;
		name << ::getcwd(0, 0) << "/";
		name << "factory-" << std::chrono::steady_clock::now().time_since_epoch().count();
		return name.str();
	}

	std::string _name;
};

struct App {
	int run(int, char**) {
		try {
//			const char* name = "node1";
//			Tmp_file conf_file(name);
//			std::cout << "LXC path = " << conf_file.name() << std::endl;
//			struct lxc_container* node1 = lxc_container_new(name, NULL);
////			node1->set_config_item(node1, "id_map", "u 0 100000 65536");
////			node1->set_config_item(node1, "id_map", "g 0 100000 65536");
//			node1->save_config(node1, NULL);
////			node1->set_config_path(node1, conf_file.name());
//			std::cout << "Config file = " << node1->configfile << std::endl;
//			std::cout << "Config path = " << node1->config_path << std::endl;
//			std::cout << "Config path = " << node1->get_config_path(node1) << std::endl;
//			if (node1 == nullptr) {
//				throw std::runtime_error("lxc_container_new() returned NULL.");
//			}
//			lxc_check("create()", node1, node1->create(node1, "/bin/true", NULL, NULL, 0, NULL));
//			lxc_check("destroy()", node1, node1->destroy(node1));
//			free(node1);
			factory::find();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};
