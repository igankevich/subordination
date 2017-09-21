#include "kernel_instance_registry.hh"

#include <algorithm>
#include <iterator>

#include "config.hh"

namespace {

	template <class T>
	struct Entry {

		typedef T kernel_type;
		typedef typename kernel_type::id_type id_type;
		typedef std::pair<const id_type, kernel_type*> value_type;

		inline
		Entry(const value_type& rhs):
		_kernel(rhs.second) {}

		inline friend std::ostream&
		operator<<(std::ostream& out, const Entry& rhs) {
			return out
				<< "/instance/"
				<< rhs._kernel->id()
				<< '='
				<< rhs.safe_type_name();
		}

	private:

		inline const char*
		safe_type_name() const {
			return !_kernel->type()
				? "<unknown>"
				: _kernel->type().name();
		}

		kernel_type* _kernel;
	};

}

template <class T>
std::ostream&
bsc::operator<<(std::ostream& out, const kernel_instance_registry<T>& rhs) {
	std::unique_lock<std::mutex> lock(rhs._mutex);
	std::ostream_iterator<Entry<T>> it(out, "\n");
	std::copy(rhs._instances.cbegin(), rhs._instances.cend(), it);
	return out;
}

bsc::instance_registry_type bsc::instances;

template class bsc::kernel_instance_registry<BSCHEDULER_KERNEL_TYPE>;
