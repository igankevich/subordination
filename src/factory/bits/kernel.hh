#ifndef FACTORY_BITS_KERNEL_HH
#define FACTORY_BITS_KERNEL_HH

namespace factory {

	namespace components {

		template<class K>
		struct Kernel_ref {

			constexpr Kernel_ref(): _kernel(nullptr), _temp(false) {}
			constexpr Kernel_ref(K* rhs): _kernel(rhs), _temp(false) {}
			Kernel_ref(Id rhs):
				_kernel(rhs == ROOT_ID ? nullptr : new Transient_kernel(rhs)),
				_temp(rhs != ROOT_ID) {}
			Kernel_ref(const Kernel_ref& rhs): _kernel(rhs._kernel), _temp(rhs._temp) {
				if (_temp) {
					Id i = _kernel->id();
					_kernel = new Transient_kernel(i);
				}
			}
			~Kernel_ref() {
				if (_temp) delete _kernel;
			}

			// dereference operators
			K* operator->() { return _kernel; }
			const K* operator->() const { return _kernel; }
			K& operator*() { return *_kernel; }
			const K& operator*() const { return *_kernel; }

			Kernel_ref& operator=(const Kernel_ref& rhs) {
				if (&rhs == this) return *this;
				if (_temp) {
					delete _kernel;
				}
				_temp = rhs._temp;
				if (_temp) {
					Id i = rhs._kernel->id();
					_kernel = new Transient_kernel(i);
				} else {
					_kernel = rhs._kernel;
				}
				return *this;
			}

			Kernel_ref& operator=(K* rhs) {
				if (rhs == _kernel) return *this;
				if (_temp) {
					delete _kernel;
					_temp = false;
				}
				_kernel = rhs;
				return *this;
			}

			Kernel_ref& operator=(Id rhs) {
				if (_temp) {
					delete _kernel;
				}
				if (rhs == ROOT_ID) {
					_kernel = nullptr;
					_temp = false;
				} else {
					_kernel = new Transient_kernel(rhs);
					_temp = true;
				}
				return *this;
			}

			K* ptr() { return _kernel; }
			const K* ptr() const { return _kernel; }

			constexpr explicit operator bool() const { return _kernel != nullptr; }
			constexpr bool operator !() const { return _kernel == nullptr; }

			friend std::ostream& operator<<(std::ostream& out, const Kernel_ref<K>& rhs) {
				return out << rhs->id();
			}

		private:
			K* _kernel;
			bool _temp;

			// No one lives forever.
			struct Transient_kernel: public Identifiable<K, Type<K>> {
				explicit Transient_kernel(Id i): Identifiable<K, Type<K>>(i, false) {}
			};
		};

	}

}
#endif // FACTORY_BITS_KERNEL_HH
