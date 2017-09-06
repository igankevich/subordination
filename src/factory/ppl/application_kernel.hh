#ifndef FACTORY_PPL_APPLICATION_KERNEL_HH
#define FACTORY_PPL_APPLICATION_KERNEL_HH

#include <cstdint>
#include <factory/config.hh>
#include <string>
#include <vector>

namespace factory {

	class Application_kernel: public FACTORY_KERNEL_TYPE {

	public:
		typedef FACTORY_KERNEL_TYPE kernel_type;
		typedef std::vector<std::string> container_type;

	private:
		container_type _args, _env;
		std::string _error;

	public:

		inline
		Application_kernel(
			const container_type& args,
			const container_type& env
		):
		_args(args),
		_env(env)
		{}

		Application_kernel() = default;
		virtual ~Application_kernel() = default;

		void
		act() override {
			this->return_to_parent();
		}

		inline const container_type&
		arguments() const noexcept {
			return this->_args;
		}

		inline const container_type&
		environment() const noexcept {
			return this->_env;
		}

		inline void
		set_error(const std::string& rhs) {
			this->_error = rhs;
		}

		const std::string
		error() const noexcept {
			return this->_error;
		}

		void
		write(sys::pstream& out) override;

		void
		read(sys::pstream& in) override;

	};

}

#endif // vim:filetype=cpp
