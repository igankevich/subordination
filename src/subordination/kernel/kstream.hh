#ifndef SUBORDINATION_KERNEL_KSTREAM_HH
#define SUBORDINATION_KERNEL_KSTREAM_HH

#include <cassert>

#include <unistdx/base/log_message>
#include <unistdx/net/socket_address>
#include <unistdx/net/pstream>

#include <subordination/base/error.hh>
#include <subordination/kernel/foreign_kernel.hh>
#include <subordination/kernel/kernel_error.hh>
#include <subordination/kernel/kernel_type_registry.hh>
#include <subordination/kernel/kernelbuf.hh>
#include <subordination/ppl/kernel_proto_flag.hh>

namespace bsc {

    namespace bits {

        template <class Router>
        struct no_forward: public Router {
            void
            operator()(foreign_kernel*) {
                assert(false);
            }
        };

        template <class Router>
        struct forward_to_child: public Router {
            void
            operator()(foreign_kernel* hdr) {
                this->forward_child(hdr);
            }
        };

        template <class Router>
        struct forward_to_parent: public Router {
            void
            operator()(foreign_kernel* hdr) {
                this->forward_parent(hdr);
            }
        };

        template <class T>
        struct no_router {

            void
            send_local(T* rhs) {}

            void
            send_remote(T*) {}

            void
            forward(foreign_kernel*) {}

            void
            forward_child(foreign_kernel*) {}

            void
            forward_parent(foreign_kernel*) {}

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
        operator<<(foreign_kernel* k) {
            return operator<<(*k);
        }

        inline kstream&
        operator<<(foreign_kernel& k) {
            this->write_foreign(k);
            return *this;
        }

        inline kstream&
        operator<<(kernel_type* k) {
            return operator<<(*k);
        }

        kstream&
        operator<<(kernel_type& k) {
            this->write_native(k);
            if (k.carries_parent()) {
                // embed parent into the packet
                kernel_type* parent = k.parent();
                if (!parent) {
                    throw std::invalid_argument("parent is null");
                }
                this->write_native(*parent);
            }
            return *this;
        }

        kstream&
        operator>>(kernel_type*& k) {
            k = this->read_native();
            if (k->carries_parent()) {
                kernel_type* parent = this->read_native();
                k->parent(parent);
            }
            return *this;
        }

        kstream&
        operator>>(foreign_kernel& k) {
            this->read_foreign(k);
            return *this;
        }

    private:

        inline void
        write_native(kernel_type& k) {
            auto type = types.find(typeid(k));
            if (type == types.end()) {
                throw std::invalid_argument("kernel type is null");
            }
            *this << type->id();
            k.write(*this);
        }

        inline kernel_type*
        read_native() {
            return types.read_object(*this);
        }

        inline void
        write_foreign(foreign_kernel& k) {
            k.write(*this);
        }

        inline void
        read_foreign(foreign_kernel& k) {
            k.read(*this);
        }

    };

}

#endif // vim:filetype=cpp
