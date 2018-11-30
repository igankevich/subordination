#ifndef BSCHEDULER_PPL_KERNEL_PROTOCOL_HH
#define BSCHEDULER_PPL_KERNEL_PROTOCOL_HH

#include <algorithm>
#include <deque>
#include <memory>

#include <unistdx/base/delete_each>
#include <unistdx/ipc/process>

#include <bscheduler/base/queue_popper.hh>
#include <bscheduler/kernel/foreign_kernel.hh>
#include <bscheduler/kernel/kernel_header.hh>
#include <bscheduler/kernel/kernel_instance_registry.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/kernel_proto_flag.hh>

namespace bsc {

	template <
		class T,
		class Router,
		class Forward=bits::no_forward<Router>,
		class Kernels=std::deque<T*>,
		class Traits=deque_traits<Kernels>>
	class kernel_protocol {

	public:
		typedef T kernel_type;
		typedef Router router_type;
		typedef Forward forward_type;
		typedef Kernels pool_type;
		typedef Traits traits_type;

	private:
		typedef kstream<T> stream_type;
		typedef queue_pop_iterator<Kernels,Traits> queue_popper;
		typedef typename T::id_type id_type;
		typedef typename stream_type::ipacket_guard ipacket_guard;
		typedef sys::opacket_guard<stream_type> opacket_guard;
		typedef std::unique_ptr<application> application_ptr;
		typedef typename pool_type::iterator kernel_iterator;

	private:
		kernel_proto_flag _flags = kernel_proto_flag(0);
		/// Endpoint from which kernels come.
		sys::socket_address _endpoint;
		/// Cluster-wide application ID.
		application_type _thisapp = this_application::get_id();
		/// Application of the kernels coming in.
		const application* _otheraptr = 0;
		pool_type _upstream;
		pool_type _downstream;
		forward_type _forward;
		id_type _counter = 0;
		const char* _name = "proto";

	public:

		kernel_protocol() = default;
		kernel_protocol(kernel_protocol&&) = default;

		kernel_protocol(const kernel_protocol&) = delete;
		kernel_protocol& operator=(const kernel_protocol&) = delete;
		kernel_protocol& operator=(kernel_protocol&&) = delete;

		~kernel_protocol() {
			sys::delete_each(queue_popper(this->_upstream), queue_popper());
			sys::delete_each(queue_popper(this->_downstream), queue_popper());
		}

		void
		send(kernel_type* k, stream_type& stream) {
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
				#ifndef NDEBUG
				this->log("send local kernel _", *k);
				#endif
				router_type::send_local(k);
				return;
			}
			bool delete_kernel = this->save_kernel(k);
			#ifndef NDEBUG
			this->log("send _ to _", *k, this->_endpoint);
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

		void
		receive_kernels(stream_type& stream) noexcept {
			while (stream.read_packet()) {
				try {
					if (kernel_type* k = this->read_kernel(stream)) {
						bool ok = this->receive_kernel(k);
						if (!ok) {
							#ifndef NDEBUG
							this->log("no principal found for _", *k);
							#endif
							k->principal(k->parent());
							this->send(k, stream);
						} else {
							router_type::send_local(k);
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
			this->do_recover_kernels(this->_upstream);
			if (down) {
				this->do_recover_kernels(this->_downstream);
			}
		}

	private:

		// send {{{
		void
		write_kernel(kernel_type* k, stream_type& stream) noexcept {
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
		do_write_kernel(kernel_type& k, stream_type& stream) {
			if (this->has_src_and_dest()) {
				k.header().prepend_source_and_destination();
			}
			stream << k.header();
			stream << k;
		}

		bool
		kernel_goes_in_upstream_buffer(const kernel_type* rhs) noexcept {
			return this->saves_upstream_kernels() &&
				   (rhs->moves_upstream() || rhs->moves_somewhere());
		}

		bool
		kernel_goes_in_downstream_buffer(const kernel_type* rhs) noexcept {
			return this->saves_downstream_kernels() &&
				   rhs->moves_downstream() &&
				   rhs->carries_parent();
		}
		// }}}

		// receive {{{
		kernel_type*
		read_kernel(stream_type& stream) {
			// eats remaining bytes on exception
			ipacket_guard g(stream.rdbuf());
			foreign_kernel* hdr = new foreign_kernel;
			kernel_type* k = nullptr;
			stream >> hdr->header();
			if (this->has_other_application()) {
				hdr->setapp(this->other_application_id());
				hdr->aptr(this->_otheraptr);
			}
			if (this->_endpoint) {
				hdr->from(this->_endpoint);
				hdr->prepend_source_and_destination();
			}
			#ifndef NDEBUG
			this->log("recv _", hdr->header());
			#endif
			if (hdr->app() != this->_thisapp) {
				stream >> *hdr;
				this->_forward(hdr);
			} else {
				stream >> k;
				k->setapp(hdr->app());
				if (hdr->has_source_and_destination()) {
					k->from(hdr->from());
					k->to(hdr->to());
				} else {
					k->from(this->_endpoint);
				}
				if (k->carries_parent()) {
					k->parent()->setapp(hdr->app());
				}
				delete hdr;
			}
			return k;
		}

		bool
		receive_kernel(kernel_type* k) {
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
			#ifndef NDEBUG
			this->log("recv _", *k);
			#endif
			return ok;
		}

		void
		plug_parent(kernel_type* k) {
			if (!k->has_id()) {
				throw std::invalid_argument("downstream kernel without an id");
			}
			kernel_iterator pos = this->find_kernel(k, this->_upstream);
			if (pos == this->_upstream.end()) {
				if (k->carries_parent()) {
					k->principal(k->parent());
					this->log("recover parent for _", *k);
					kernel_iterator result2 =
						this->find_kernel(k, this->_downstream);
					if (result2 != this->_downstream.end()) {
						kernel_type* old = *result2;
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
				kernel_type* orig = *pos;
				k->parent(orig->parent());
				k->principal(k->parent());
				delete orig;
				this->_upstream.erase(pos);
				#ifndef NDEBUG
				this->log("plug parent for _", *k);
				#endif
			}
		}

		kernel_iterator
		find_kernel(kernel_type* k, pool_type& pool) {
			return std::find_if(
				pool.begin(),
				pool.end(),
				[k] (kernel_type* rhs) { return rhs->id() == k->id(); }
			);
		}
		// }}}

		// recover {{{
		bool
		save_kernel(kernel_type* k) {
			bool delete_kernel = false;
			if (kernel_goes_in_upstream_buffer(k)) {
				if (k->is_native()) {
					this->ensure_has_id(k->parent());
					this->ensure_has_id(k);
				}
				#ifndef NDEBUG
				this->log("save parent for _", *k);
				#endif
				traits_type::push(this->_upstream, k);
			} else
			if (kernel_goes_in_downstream_buffer(k)) {
				#ifndef NDEBUG
				this->log("save parent for _", *k);
				#endif
				traits_type::push(this->_downstream, k);
			} else
			if (!k->moves_everywhere()) {
				delete_kernel = true;
			}
			return delete_kernel;
		}

		void
		do_recover_kernels(pool_type& rhs) noexcept {
			using namespace std::placeholders;
			std::for_each(
				queue_popper(rhs),
				queue_popper(rhs),
				[this] (kernel_type* rhs) {
					try {
						this->recover_kernel(rhs);
					} catch (const std::exception& err) {
						this->log("failed to recover kernel _", *rhs);
						delete rhs;
					}
				}
			);
		}

		void
		recover_kernel(kernel_type* k) {
			const bool native = k->is_native();
			if (k->moves_upstream() && !k->to()) {
				#ifndef NDEBUG
				this->log("recover _", *k);
				#endif
				if (native) {
					router_type::send_remote(k);
				} else {
					router_type::forward_parent(dynamic_cast<foreign_kernel*>(k));
				}
			} else if (k->moves_somewhere() || (k->moves_upstream() && k->to())) {
				#ifndef NDEBUG
				this->log("destination is unreachable for _", *k);
				#endif
				k->from(k->to());
				k->return_code(exit_code::endpoint_not_connected);
				k->principal(k->parent());
				if (native) {
					router_type::send_local(k);
				} else {
					this->_forward(dynamic_cast<foreign_kernel*>(k));
				}
			} else if (k->moves_downstream() && k->carries_parent()) {
				#ifndef NDEBUG
				this->log("restore parent _", *k);
				#endif
				if (native) {
					router_type::send_local(k);
				} else {
					this->_forward(dynamic_cast<foreign_kernel*>(k));
				}
			} else {
				this->log("bad kernel in sent buffer: _", *k);
				delete k;
			}
		}
		// }}}

		void
		ensure_has_id(kernel_type* k) {
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

		inline void
		set_name(const char* rhs) noexcept {
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

		inline bool
		has_other_application() const noexcept {
			return this->_otheraptr;
		}

		inline void
		set_other_application(const application* rhs) noexcept {
			this->_otheraptr = rhs;
		}

		inline application_type
		other_application_id() const noexcept {
			return this->_otheraptr->id();
		}

		inline void
		set_endpoint(const sys::socket_address& rhs) noexcept {
			this->_endpoint = rhs;
		}

		inline const sys::socket_address&
		socket_address() const noexcept {
			return this->_endpoint;
		}

	};

}

#endif // vim:filetype=cpp
