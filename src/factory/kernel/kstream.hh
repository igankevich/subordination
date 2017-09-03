#ifndef FACTORY_KERNEL_KSTREAM_HH
#define FACTORY_KERNEL_KSTREAM_HH

#include <unistdx/base/log_message>
#include <unistdx/net/pstream>

#include <factory/base/error.hh>
#include <factory/kernel/kernel_type_registry.hh>
#include <factory/kernel/kernel_error.hh>
#include <factory/kernel/kernelbuf.hh>

namespace factory {

	template<class T>
	struct kstream: public sys::pstream {

		typedef T kernel_type;
		typedef typename kernel_type::app_type app_type;
		typedef std::function<void(app_type)> forward_func;

		enum struct state {
			good = 0,
			partial_packet,
			bad_kernel
		};

		using sys::pstream::operator<<;
		using sys::pstream::operator>>;

		kstream() = default;
		inline explicit
		kstream(sys::packetbuf* buf): sys::pstream(buf) {}
		kstream(kstream&&) = default;

		inline kstream&
		operator<<(kernel_type* kernel) {
			return operator<<(*kernel);
		}

		kstream&
		operator<<(kernel_type& kernel) {
			kernel_type_registry::const_iterator type = types.find(typeid(kernel));
			if (type == types.end()) {
				throw std::invalid_argument("kernel type is null");
			}
			begin_packet();
			*this << kernel.app() << type->id();
			kernel.write(*this);
			if (kernel.carries_parent()) {
				// embed parent into the packet
				kernel_type* parent = kernel.parent();
				if (!parent) {
					throw std::invalid_argument("parent is null");
				}
				kernel_type_registry::const_iterator parent_type = types.find(typeid(*parent));
				if (parent_type == types.end()) {
					throw std::invalid_argument("parent type is null");
				}
				*this << parent_type->id();
				parent->write(*this);
			}
			end_packet();
			return *this;
		}

		kstream&
		operator>>(kernel_type*& kernel) {
			if (!read_packet()) {
				this->setstate(state::partial_packet);
			} else {
				app_type app;
				*this >> app;
				if (app != this->_thisapp) {
					this->_doforward(app);
				} else {
					try {
						kernel = this->read_kernel();
						kernel->setapp(app);
						if (kernel->carries_parent()) {
							kernel_type* parent = this->read_kernel();
							parent->setapp(app);
							kernel->parent(parent);
						}
					} catch (const kernel_error& err) {
						this->setstate(state::bad_kernel);
						throw;
					}
					// eat remaining bytes
					this->rdbuf()->skip_packet();
				}

			}
			return *this;
		}

		inline void
		setforward(const forward_func& rhs) noexcept {
			this->_doforward = rhs;
		}

		inline void
		setstate(state rhs) noexcept {
			this->_rdstate = rhs;
		}

		inline state
		rdstate() const noexcept {
			return this->_rdstate;
		}

		inline explicit
		operator bool() const noexcept {
			return this->good();
		}

		inline bool
		operator!() const noexcept {
			return !this->operator bool();
		}

		inline bool
		partial() const noexcept {
			return this->_rdstate == state::partial_packet;
		}

		inline bool
		good() const noexcept {
			return this->_rdstate == state::good;
		}

		inline bool
		bad() const noexcept {
			return this->_rdstate == state::bad_kernel;
		}

		inline void
		clear() noexcept {
			this->_rdstate = state::good;
		}

		inline void
		setapp(app_type rhs) noexcept {
			this->_thisapp = rhs;
		}

	private:

		inline friend std::ostream&
		operator<<(std::ostream& out, state rhs) {
			switch (rhs) {
				case state::good: out << "good"; break;
				case state::partial_packet: out << "partial_packet"; break;
				case state::bad_kernel: out << "bad_kernel"; break;
			}
			return out;
		}

		inline kernel_type*
		read_kernel() {
			return static_cast<kernel_type*>(types.read_object(*this));
		}

		app_type _thisapp = 0;
		forward_func _doforward;
		state _rdstate = state::good;

	};

}

#endif // vim:filetype=cpp