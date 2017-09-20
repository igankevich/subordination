#include "external_process_handler.hh"

#include <factory/config.hh>
#include <factory/ppl/basic_router.hh>

template <class K, class R>
void
factory::external_process_handler<K,R>
::handle(sys::poll_event& event) {
	if (this->is_starting()) {
		this->setstate(pipeline_state::started);
	}
	this->_stream.sync();
	this->receive_kernels(this->_stream);
	this->_stream.sync();
}

template <class K, class R>
void
factory::external_process_handler<K,R>
::receive_kernels(stream_type& stream) noexcept {
	while (stream.read_packet()) {
		Application_kernel* k = nullptr;
		try {
			// eats remaining bytes on exception
			application_type a;
			ipacket_guard g(stream);
			kernel_type* kernel = nullptr;
			stream >> a;
			stream >> kernel;
			k = dynamic_cast<Application_kernel*>(kernel);
			this->log("recv _", *k);
			Application app(k->arguments(), k->environment());
			sys::user_credentials creds = this->socket().credentials();
			app.set_credentials(creds.uid, creds.gid);
			try {
				router_type::execute(app);
				k->return_to_parent(exit_code::success);
			} catch (const std::exception& err) {
				k->return_to_parent(exit_code::error);
				k->set_error(err.what());
			} catch (...) {
				k->return_to_parent(exit_code::error);
				k->set_error("unknown error");
			}
		} catch (const Error& err) {
			this->log("read error _", err);
		} catch (const std::exception& err) {
			this->log("read error _ ", err.what());
		} catch (...) {
			this->log("read error _", "<unknown>");
		}
		if (k) {
			try {
				opacket_guard g(stream);
				stream.begin_packet();
				stream << this_application::get_id();
				stream << k;
				stream.end_packet();
			} catch (const Error& err) {
				this->log("write error _", err);
			} catch (const std::exception& err) {
				this->log("write error _", err.what());
			} catch (...) {
				this->log("write error _", "<unknown>");
			}
			delete k;
		}
	}
}


template class factory::external_process_handler<
		FACTORY_KERNEL_TYPE,factory::basic_router<FACTORY_KERNEL_TYPE>>;
