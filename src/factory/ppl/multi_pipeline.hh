#ifndef FACTORY_PPL_MULTI_PIPELINE_HH
#define FACTORY_PPL_MULTI_PIPELINE_HH

#include <factory/ppl/basic_server.hh>
#include <factory/ppl/basic_cpu_server.hh>
#include <vector>

namespace factory {

	template <class T>
	class Multi_pipeline: public Server_base {

	public:
		typedef T kernel_type;
		typedef Basic_CPU_server<T> base_server;

	private:
		std::vector<base_server> _servers;

	public:
		explicit
		Multi_pipeline(unsigned nservers);
		Multi_pipeline(const Multi_pipeline&) = delete;
		Multi_pipeline(Multi_pipeline&&) = default;
		virtual ~Multi_pipeline() = default;

		inline base_server&
		operator[](size_t i) noexcept {
			return this->_servers[i];
		}

		inline const base_server&
		operator[](size_t i) const noexcept {
			return this->_servers[i];
		}

		inline size_t
		size() const noexcept {
			return this->_servers.size();
		}

		void
		set_name(const char* rhs);

		void
		start();

		void
		stop();

		void
		wait();
	};

}

#endif // FACTORY_PPL_MULTI_PIPELINE_HH vim:filetype=cpp
