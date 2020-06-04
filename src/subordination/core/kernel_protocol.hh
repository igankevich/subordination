#ifndef SUBORDINATION_CORE_KERNEL_PROTOCOL_HH
#define SUBORDINATION_CORE_KERNEL_PROTOCOL_HH

#include <algorithm>
#include <deque>
#include <memory>

#include <unistdx/base/delete_each>
#include <unistdx/ipc/process>

#include <subordination/core/application.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel_header.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_proto_flag.hh>
#include <subordination/core/kstream.hh>
#include <subordination/core/pipeline_base.hh>

namespace sbn {

    class kernel_protocol {

    public:
        using kernel_queue = std::deque<kernel*>;

    private:
        using stream_type = kstream;
        using id_type = typename kernel::id_type;
        using ipacket_guard = typename stream_type::ipacket_guard;
        using opacket_guard = sys::opacket_guard<stream_type>;
        using kernel_iterator = typename kernel_queue::iterator;

    private:
        kernel_proto_flag _flags{};
        /// Cluster-wide application ID.
        application_type _thisapp = this_application::get_id();
        kernel_queue _upstream, _downstream;
        pipeline* _foreign_pipeline = nullptr;
        pipeline* _native_pipeline = nullptr;
        pipeline* _remote_pipeline = nullptr;
        id_type _counter = 0;
        const char* _name = "proto";

    public:

        kernel_protocol() = default;
        kernel_protocol(kernel_protocol&&) = default;
        kernel_protocol(const kernel_protocol&) = delete;
        kernel_protocol& operator=(const kernel_protocol&) = delete;
        kernel_protocol& operator=(kernel_protocol&&) = delete;
        ~kernel_protocol() = default;

        void
        send(kernel* k, stream_type& stream, const sys::socket_address& to) {
            // return local downstream kernels immediately
            // TODO we need to move some kernel flags to
            // kernel header in order to use them in routing
            if (k->moves_downstream() && !k->to()) {
                if (k->isset(kernel_flag::parent_is_id) || k->carries_parent()) {
                    if (k->carries_parent()) {
                        delete k->parent();
                    }
                    this->plug_parent(k);
                }
                #if defined(SBN_DEBUG)
                this->log("send local kernel _", *k);
                #endif
                this->_native_pipeline->send(k);
                return;
            }
            bool delete_kernel = this->save_kernel(k);
            #if defined(SBN_DEBUG)
            this->log("send _ to _", *k, to);
            #endif
            this->write_kernel(k, stream);
            /// The kernel is deleted if it goes downstream
            /// and does not carry its parent.
            if (delete_kernel) {
                if (k->moves_downstream() && k->carries_parent()) {
                    delete k->parent();
                }
                delete k;
            }
        }

        void
        forward(foreign_kernel* k, stream_type& ostr) {
            bool delete_kernel = this->save_kernel(k);
            ostr.begin_packet();
            ostr << k->header();
            ostr << *k;
            ostr.end_packet();
            if (delete_kernel) {
                delete k;
            }
        }

        /// \param[in] from socket address from which kernels are received
        void receive_kernels(stream_type& stream, const sys::socket_address& from,
                             const application* from_application=nullptr) noexcept {
            this->receive_kernels(stream, from, from_application, [] (kernel*) {});
        }

        template <class Callback> void
        receive_kernels(stream_type& stream,
                        const sys::socket_address& from,
                        const application* from_application,
                        Callback func) noexcept {
            while (stream.read_packet()) {
                try {
                    if (auto* k = this->read_kernel(stream, from, from_application)) {
                        bool ok = this->receive_kernel(k);
                        func(k);
                        if (!ok) {
                            #if defined(SBN_DEBUG)
                            this->log("no principal found for _", *k);
                            #endif
                            k->principal(k->parent());
                            this->send(k, stream, from);
                        } else {
                            this->_native_pipeline->send(k);
                        }
                    }
                } catch (const kernel_error& err) {
                    log_read_error(err);
                } catch (const error& err) {
                    log_read_error(err);
                } catch (const std::exception& err) {
                    log_read_error(err.what());
                } catch (...) {
                    log_read_error("<unknown>");
                }
            }
        }

        void
        recover_kernels(bool down) {
            #if defined(SBN_DEBUG)
            this->log("recover kernels upstream _ downstream _",
                      this->_upstream.size(), this->_downstream.size());
            #endif
            this->do_recover_kernels(this->_upstream);
            if (down) {
                this->do_recover_kernels(this->_downstream);
            }
        }

    private:

        void
        write_kernel(kernel* k, stream_type& stream) noexcept {
            try {
                opacket_guard g(stream);
                stream.begin_packet();
                this->do_write_kernel(*k, stream);
                stream.end_packet();
            } catch (const kernel_error& err) {
                log_write_error(err);
            } catch (const error& err) {
                log_write_error(err);
            } catch (const std::exception& err) {
                log_write_error(err.what());
            } catch (...) {
                log_write_error("<unknown>");
            }
        }

        void
        do_write_kernel(kernel& k, stream_type& stream) {
            if (this->has_src_and_dest()) {
                k.header().prepend_source_and_destination();
            }
            stream << k.header();
            stream << k;
        }

        bool
        kernel_goes_in_upstream_buffer(const kernel* rhs) noexcept {
            return this->saves_upstream_kernels() &&
                   (rhs->moves_upstream() || rhs->moves_somewhere());
        }

        bool
        kernel_goes_in_downstream_buffer(const kernel* rhs) noexcept {
            return this->saves_downstream_kernels() &&
                   rhs->moves_downstream() &&
                   rhs->carries_parent();
        }

        kernel*
        read_kernel(stream_type& stream,
                    const sys::socket_address& from,
                    const application* from_application) {
            // eats remaining bytes on exception
            ipacket_guard g(stream.rdbuf());
            foreign_kernel* hdr = new foreign_kernel;
            kernel* k = nullptr;
            stream >> hdr->header();
            if (from_application) {
                hdr->setapp(from_application->id());
                hdr->aptr(from_application);
            }
            if (from) {
                hdr->from(from);
                hdr->prepend_source_and_destination();
            }
            #if defined(SBN_DEBUG)
            this->log("recv _", hdr->header());
            #endif
            if (hdr->app() != this->_thisapp) {
                stream >> *hdr;
                this->_foreign_pipeline->forward(hdr);
            } else {
                stream >> k;
                k->setapp(hdr->app());
                if (hdr->has_source_and_destination()) {
                    k->from(hdr->from());
                    k->to(hdr->to());
                } else {
                    k->from(from);
                }
                if (k->carries_parent()) {
                    k->parent()->setapp(hdr->app());
                }
                delete hdr;
            }
            return k;
        }

        bool
        receive_kernel(kernel* k) {
            bool ok = true;
            if (k->moves_downstream()) {
                this->plug_parent(k);
            } else if (k->principal_id()) {
                instances_guard g(instances);
                auto result = instances.find(k->principal_id());
                if (result == instances.end()) {
                    k->return_code(exit_code::no_principal_found);
                    ok = false;
                }
                k->principal(result->second);
            }
            #if defined(SBN_DEBUG)
            this->log("recv _", *k);
            #endif
            return ok;
        }

        void
        plug_parent(kernel* k) {
            if (!k->has_id()) {
                throw std::invalid_argument("downstream kernel without an id");
            }
            auto pos = this->find_kernel(k, this->_upstream);
            if (pos == this->_upstream.end()) {
                if (k->carries_parent()) {
                    k->principal(k->parent());
                    this->log("recover parent for _", *k);
                    auto result2 = this->find_kernel(k, this->_downstream);
                    if (result2 != this->_downstream.end()) {
                        kernel* old = *result2;
                        this->log("delete _", *old);
                        delete old->parent();
                        delete old;
                        this->_downstream.erase(result2);
                    }
                } else {
                    this->log("parent not found for _", *k);
                    delete k;
                    throw std::invalid_argument("parent not found");
                }
            } else {
                kernel* orig = *pos;
                k->parent(orig->parent());
                k->principal(k->parent());
                delete orig;
                this->_upstream.erase(pos);
                #if defined(SBN_DEBUG)
                this->log("plug parent for _", *k);
                #endif
            }
        }

        kernel_iterator
        find_kernel(kernel* k, kernel_queue& pool) {
            return std::find_if(
                pool.begin(),
                pool.end(),
                [k] (kernel* rhs) { return rhs->id() == k->id(); }
            );
        }

        bool
        save_kernel(kernel* k) {
            bool delete_kernel = false;
            if (kernel_goes_in_upstream_buffer(k)) {
                if (k->is_native()) {
                    this->ensure_has_id(k->parent());
                    this->ensure_has_id(k);
                }
                #if defined(SBN_DEBUG)
                this->log("save parent for _", *k);
                #endif
                this->_upstream.push_back(k);
            } else
            if (kernel_goes_in_downstream_buffer(k)) {
                #if defined(SBN_DEBUG)
                this->log("save parent for _", *k);
                #endif
                this->_downstream.push_back(k);
            } else
            if (!k->moves_everywhere()) {
                delete_kernel = true;
            }
            return delete_kernel;
        }

        void
        do_recover_kernels(kernel_queue& rhs) noexcept {
            using namespace std::placeholders;
            while (!rhs.empty()) {
                auto* k = rhs.front();
                rhs.pop_front();
                try {
                    this->recover_kernel(k);
                } catch (const std::exception& err) {
                    this->log("failed to recover kernel _", *k);
                    delete k;
                }
            }
        }

        void recover_kernel(kernel* k);

        void
        ensure_has_id(kernel* k) {
            if (!k->has_id()) {
                k->id(this->generate_id());
            }
        }

        id_type
        generate_id() noexcept {
            return ++this->_counter;
        }

        template <class E>
        void
        log_write_error(const E& err) {
            this->log("write error _", err);
        }

        template <class E>
        void
        log_read_error(const E& err) {
            this->log("read error _", err);
        }

        template <class ... Args>
        inline void
        log(const Args& ... args) {
            sys::log_message(this->_name, args ...);
        }

    public:

        void clear();

        inline void
        name(const char* rhs) noexcept {
            this->_name = rhs;
        }

        inline void
        setf(kernel_proto_flag rhs) noexcept {
            this->_flags |= rhs;
        }

        inline void
        unsetf(kernel_proto_flag rhs) noexcept {
            this->_flags &= ~rhs;
        }

        inline kernel_proto_flag
        flags() const noexcept {
            return this->_flags;
        }

        inline bool
        has_src_and_dest() const noexcept {
            return this->_flags &
                kernel_proto_flag::prepend_source_and_destination;
        }

        inline bool
        prepends_application() const noexcept {
            return this->_flags & kernel_proto_flag::prepend_application;
        }

        inline bool
        saves_upstream_kernels() const noexcept {
            return this->_flags & kernel_proto_flag::save_upstream_kernels;
        }

        inline bool
        saves_downstream_kernels() const noexcept {
            return this->_flags & kernel_proto_flag::save_downstream_kernels;
        }

        inline const pipeline* foreign_pipeline() const noexcept { return this->_foreign_pipeline; }
        inline pipeline* foreign_pipeline() noexcept { return this->_foreign_pipeline; }
        inline void foreign_pipeline(pipeline* rhs) noexcept { this->_foreign_pipeline = rhs; }
        inline const pipeline* native_pipeline() const noexcept { return this->_native_pipeline; }
        inline pipeline* native_pipeline() noexcept { return this->_native_pipeline; }
        inline void native_pipeline(pipeline* rhs) noexcept { this->_native_pipeline = rhs; }
        inline const pipeline* remote_pipeline() const noexcept { return this->_remote_pipeline; }
        inline pipeline* remote_pipeline() noexcept { return this->_remote_pipeline; }
        inline void remote_pipeline(pipeline* rhs) noexcept { this->_remote_pipeline = rhs; }

    };

}

#endif // vim:filetype=cpp
