#ifndef FACTORY_PPL_KERNEL_PROTOCOL_HH
#define FACTORY_PPL_KERNEL_PROTOCOL_HH

#include <algorithm>
#include <deque>
#include <factory/kernel/kernel_header.hh>
#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/kernel_proto_flag.hh>
#include <unistdx/base/delete_each>
#include <unistdx/it/queue_popper>

namespace factory {

	template <
		class T,
		class Router,
		class Forward=bits::no_forward<Router>,
		class Kernels=std::deque<T*>,
		class Traits=sys::deque_traits<Kernels>>
	class kernel_protocol {

	public:
		typedef T kernel_type;
		typedef Router router_type;
		typedef Forward forward_type;
		typedef Kernels pool_type;
		typedef Traits traits_type;
		typedef kstream<T> stream_type;
		typedef sys::queue_pop_iterator<Kernels,Traits> queue_popper;

	private:
		kernel_proto_flag _flags = kernel_proto_flag(0);
		/// Endpoint from which kernels come.
		sys::endpoint _endpoint;
		/// Cluster-wide application ID.
		application_type _thisapp = this_application::get_id();
		pool_type _upstream;
		pool_type _downstream;
		forward_type _forward;

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
		send(kernel_type* kernel, stream_type& stream) {
			bool delete_kernel = false;
			if (kernel_goes_in_upstream_buffer(kernel)) {
				traits_type::push(this->_upstream, kernel);
			} else
			if (kernel_goes_in_downstream_buffer(kernel)) {
				traits_type::push(this->_downstream, kernel);
			} else
			if (!kernel->moves_everywhere()) {
				delete_kernel = true;
			}
			sys::log_message(
				"proto",
				"send to _ kernel _",
				this->_endpoint,
				*kernel
			);
			this->write_kernel(kernel, stream);
			/// The kernel is deleted if it goes downstream
			/// and does not carry its parent.
			if (delete_kernel) {
				delete kernel;
			}
		}

		void
		forward(
			const kernel_header& hdr,
			sys::pstream& istr,
			stream_type& ostr
		) {
			ostr.begin_packet();
			ostr.append_payload(istr);
			ostr.end_packet();
			#ifndef NDEBUG
			sys::log_message("proto", "forward _", hdr);
			#endif
		}

		void
		receive_kernels(stream_type& stream) noexcept {
			while (stream.read_packet()) {
				try {
					try {
						if (kernel_type* k = this->read_kernel(stream)) {
							bool ok = this->receive_kernel(k);
							if (!ok) {
								#ifndef NDEBUG
								sys::log_message(
									"proto",
									"no principal found for _",
									*k
								);
								#endif
								k->principal(k->parent());
								this->send(k, stream);
							} else {
								router_type::send_local(k);
							}
						}
					} catch (...) {
						// eat remaining bytes
						stream.skip_packet();
						throw;
					}
				} catch (const Error& err) {
					sys::log_message("proto", "read error _ app=_", err, _thisapp);
				} catch (const std::exception& err) {
					sys::log_message("proto", "read error _ app=_", err.what(), _thisapp);
				} catch (...) {
					sys::log_message("proto", "read error _", "<unknown>");
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
		write_kernel(kernel_type* kernel, stream_type& stream) noexcept {
			try {
				stream.begin_packet();
				try {
					this->do_write_kernel(*kernel, stream);
				} catch (...) {
					stream.rdbuf()->cancel_packet();
					throw;
				}
				stream.end_packet();
			} catch (const Error& err) {
				sys::log_message("proto", "write error _", err);
			} catch (const std::exception& err) {
				sys::log_message("proto", "write error _", err.what());
			} catch (...) {
				sys::log_message("proto", "write error _", "<unknown>");
			}
		}

		void
		do_write_kernel(kernel_type& kernel, stream_type& stream) {
			stream << kernel.app();
			if (this->has_src_and_dest()) {
				stream << kernel.from() << kernel.to();
			}
			stream << kernel;
		}

		static bool
		kernel_goes_in_upstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_upstream() || rhs->moves_somewhere();
		}

		static bool
		kernel_goes_in_downstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_downstream() && rhs->carries_parent();
		}
		// }}}

		// receive {{{
		kernel_type*
		read_kernel(stream_type& stream) {
			kernel_type* kernel = nullptr;
			application_type app;
			stream >> app;
			if (app != this->_thisapp) {
				kernel_header hdr;
				hdr.from(this->_endpoint);
				hdr.setapp(app);
				#ifndef NDEBUG
				sys::log_message(
					"proto",
					"fwd _ _ app=_",
					hdr,
					typeid(_forward).name(),
					_thisapp
				);
				#endif
				this->_forward(hdr, stream);
			} else {
				const bool b = this->has_src_and_dest();
				sys::endpoint from, to;
				if (b) {
					stream >> from >> to;
				}
				stream >> kernel;
				kernel->setapp(app);
				if (b) {
					kernel->from(from);
					kernel->to(to);
				} else {
					kernel->from(this->_endpoint);
				}
				if (kernel->carries_parent()) {
					kernel->parent()->setapp(app);
				}
			}
			return kernel;
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
					k->result(exit_code::no_principal_found);
					ok = false;
				}
				k->principal(result->second);
			}
			#ifndef NDEBUG
			sys::log_message("proto", "recv _", *k);
			#endif
			return ok;
		}

		void
		plug_parent(kernel_type* k) {
			auto pos = std::find_if(
				this->_upstream.begin(),
				this->_upstream.end(),
				[k] (kernel_type* rhs) { return *rhs == *k; }
			);
			if (pos != this->_upstream.end()) {
				kernel_type* orig = *pos;
				k->parent(orig->parent());
				k->principal(k->parent());
				delete orig;
				this->_upstream.erase(pos);
			}
		}
		// }}}

		// recover {{{
		void
		do_recover_kernels(pool_type& rhs) noexcept {
			using namespace std::placeholders;
			std::for_each(
				queue_popper(rhs),
				queue_popper(rhs),
				[this] (kernel_type* rhs) {
					this->recover_kernel(rhs);
				}
			);
		}

		void
		recover_kernel(kernel_type* k) {
			if (k->moves_upstream()) {
				#ifndef NDEBUG
				sys::log_message("proto", "recover _", *k);
				#endif
				router_type::send_remote(k);
			} else if (k->moves_somewhere()) {
				#ifndef NDEBUG
				sys::log_message("proto", "destination is unreachable for _", *k);
				#endif
				k->from(k->to());
				k->result(exit_code::endpoint_not_connected);
				k->principal(k->parent());
				router_type::send_local(k);
			} else if (k->moves_downstream() && k->carries_parent()) {
				#ifndef NDEBUG
				sys::log_message("proto", "restore parent _", *k);
				#endif
				router_type::send_local(k);
			} else {
				sys::log_message("proto", "bad kernel in sent buffer: _", *k);
			}
		}
		// }}}

	public:

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

		inline void
		set_endpoint(const sys::endpoint& rhs) noexcept {
			this->_endpoint = rhs;
		}

		inline const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

	};

}

#endif // vim:filetype=cpp
