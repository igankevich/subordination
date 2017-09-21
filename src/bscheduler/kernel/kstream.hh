#ifndef BSCHEDULER_KERNEL_KSTREAM_HH
#define BSCHEDULER_KERNEL_KSTREAM_HH

#include <cassert>

#include <unistdx/base/log_message>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>

#include <bscheduler/base/error.hh>
#include <bscheduler/kernel/kernel_error.hh>
#include <bscheduler/kernel/kernel_header.hh>
#include <bscheduler/kernel/kernel_type_registry.hh>
#include <bscheduler/kernel/kernelbuf.hh>
#include <bscheduler/ppl/kernel_proto_flag.hh>

namespace bsc {

	namespace bits {

		template <class Router>
		struct no_forward: public Router {
			void
			operator()(const kernel_header&, sys::pstream&) {
				assert("no forwarding");
			}
		};

		template <class Router>
		struct forward_to_child: public Router {
			void
			operator()(const kernel_header& hdr, sys::pstream& istr) {
				this->forward_child(hdr, istr);
			}
		};

		template <class Router>
		struct forward_to_parent: public Router {
			void
			operator()(const kernel_header& hdr, sys::pstream& istr) {
				this->forward_parent(hdr, istr);
			}
		};

		template <class T>
		struct no_router {

			void
			send_local(T* rhs) {}

			void
			send_remote(T*) {}

			void
			forward(const kernel_header&, sys::pstream&) {}

			void
			forward_child(const kernel_header&, sys::pstream&) {}

			void
			forward_parent(const kernel_header&, sys::pstream&) {}

		};
	}

	template<class T>
	struct kstream: public sys::pstream {

		typedef T kernel_type;

		using sys::pstream::operator<<;
		using sys::pstream::operator>>;

		kstream() = default;
		inline explicit
		kstream(sys::packetbuf* buf): sys::pstream(buf) {}
		kstream(kstream&&) = default;

		inline kstream&
		operator<<(kernel_type* k) {
			return operator<<(*k);
		}

		kstream&
		operator<<(kernel_type& k) {
			this->write_kernel(k);
			if (k.carries_parent()) {
				// embed parent into the packet
				kernel_type* parent = k.parent();
				if (!parent) {
					throw std::invalid_argument("parent is null");
				}
				this->write_kernel(*parent);
			}
			return *this;
		}

		kstream&
		operator>>(kernel_type*& k) {
			k = this->read_kernel();
			if (k->carries_parent()) {
				kernel_type* parent = this->read_kernel();
				k->parent(parent);
			}
			return *this;
		}

	private:

		inline void
		write_kernel(kernel_type& k) {
			auto type = types.find(typeid(k));
			if (type == types.end()) {
				throw std::invalid_argument("kernel type is null");
			}
			*this << type->id();
			k.write(*this);
		}

		inline kernel_type*
		read_kernel() {
			return static_cast<kernel_type*>(types.read_object(*this));
		}

	};

}

#endif // vim:filetype=cpp
