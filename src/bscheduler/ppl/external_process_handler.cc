#include "external_process_handler.hh"

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_router.hh>

template <class K, class R>
void
bsc::external_process_handler<K,R>
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
bsc::external_process_handler<K,R>
::receive_kernels(stream_type& stream) noexcept {
	while (stream.read_packet()) {
		Application_kernel* k = nullptr;
		try {
			// eats remaining bytes on exception
			application_type a;
			ipacket_guard g(stream);
			kernel_type* tmp = nullptr;
			stream >> a;
			stream >> tmp;
			k = dynamic_cast<Application_kernel*>(tmp);
			this->log("recv _", *k);
			application app(k->arguments(), k->environment());
			sys::user_credentials creds = this->socket().credentials();
			app.workdir(k->workdir());
			app.set_credentials(creds.uid, creds.gid);
			try {
				k->application(app.id());
				router_type::execute(app);
				k->return_to_parent(exit_code::success);
			} catch (const std::exception& err) {
				k->return_to_parent(exit_code::error);
				k->set_error(err.what());
			} catch (...) {
				k->return_to_parent(exit_code::error);
				k->set_error("unknown error");
			}
		} catch (const error& err) {
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
			} catch (const error& err) {
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


template class bsc::external_process_handler<
		BSCHEDULER_KERNEL_TYPE,bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
