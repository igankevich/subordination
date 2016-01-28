#include <string>
#include <sstream>
#include <fstream>

#include <stdx/log.hh>

namespace discovery {

	template<class T>
	struct Cache_guard {

		typedef stdx::log<Cache_guard> this_log;

		template<class Key>
		Cache_guard(const Key& key, T& obj):
		_filename(generate_filename(key)),
		_object(obj)
		{ this->load(); }

		Cache_guard(const Cache_guard&) = delete;
		Cache_guard& operator=(const Cache_guard&) = delete;
		Cache_guard(Cache_guard&&) = default;

		~Cache_guard() { this->store(); }

		bool
		load() {
			stdx::log_func<this_log>(__func__, "filename", _filename);
			std::ifstream in(_filename);
			if (in.is_open()) {
				in >> _object;
			}
			return static_cast<bool>(in);
		}

		bool
		store() {
			std::ofstream out(_filename);
			if (out.is_open()) {
				out << _object;
			}
			return static_cast<bool>(out);
		}

	private:

		template<class Key>
		std::string
		generate_filename(const Key& key) const {
			std::stringstream s;
			s << cache_directory << '/' << key;
			return s.str();
		}

		std::string _filename;
		T& _object;

		static constexpr const char* cache_directory = "/tmp";
	};

}
