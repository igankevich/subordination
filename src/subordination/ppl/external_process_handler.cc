#include <subordination/ppl/external_process_handler.hh>

#include <subordination/ppl/basic_router.hh>

template <class K, class R>
void
sbn::external_process_handler<K,R>
::handle(sys::epoll_event& event) {
    if (this->is_starting()) {
        this->setstate(pipeline_state::started);
        event.setev(sys::event::out);
    }
    this->_stream.sync();
    this->receive_kernels(this->_stream);
    this->_stream.sync();
//	if (this->_buffer->dirty()) {
//		event.setev(sys::event::out);
//	} else {
//		event.unsetev(sys::event::out);
//	}
}

template <class K, class R>
void
sbn::external_process_handler<K,R>
::receive_kernels(stream_type& stream) noexcept {
    while (stream.read_packet()) {
        Application_kernel* k = nullptr;
        try {
            // eats remaining bytes on exception
            kernel_header::application_ptr ptr = nullptr;
            kernel_header hdr;
            ipacket_guard g(stream.rdbuf());
            kernel_type* tmp = nullptr;
            stream >> hdr;
            stream >> tmp;
            k = dynamic_cast<Application_kernel*>(tmp);
            #ifndef NDEBUG
            this->log("recv _", *k);
            #endif
            application app(k->arguments(), k->environment());
            sys::user_credentials creds = this->socket().credentials();
            app.workdir(k->workdir());
            app.set_credentials(creds.uid, creds.gid);
            app.make_master();
            try {
                k->application(app.id());
                router_type::execute(app);
                k->return_to_parent(exit_code::success);
            } catch (const sys::bad_call& err) {
                k->return_to_parent(exit_code::error);
                k->set_error(err.what());
                this->log("execute error _,app=_", err, app.id());
            } catch (const std::exception& err) {
                k->return_to_parent(exit_code::error);
                k->set_error(err.what());
                this->log("execute error _,app=_", err.what(), app.id());
            } catch (...) {
                k->return_to_parent(exit_code::error);
                k->set_error("unknown error");
                this->log("execute error _", "<unknown>");
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
                kernel_header hdr;
                hdr.setapp(this_application::get_id());
                stream << hdr;
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


template class sbn::external_process_handler<sbn::kernel,sbn::basic_router<sbn::kernel>>;
