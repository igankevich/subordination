#include <subordination/core/application.hh>
#include <subordination/core/error.hh>
#include <subordination/daemon/application_kernel.hh>
#include <subordination/daemon/factory.hh>

namespace {

    void
    write_vector(sbn::kernel_buffer& out, const std::vector<std::string>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (uint32_t i=0; i<n; ++i) {
            out << rhs[i];
        }
    }

    void
    read_vector(sbn::kernel_buffer& in, std::vector<std::string>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.resize(n);
        for (uint32_t i=0; i<n; ++i) {
            in >> rhs[i];
        }
    }

}

void
sbnd::application_kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    write_vector(out, this->_args);
    write_vector(out, this->_env);
    out << this->_working_directory << this->_error << this->_application;
}

void
sbnd::application_kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    read_vector(in, this->_args);
    read_vector(in, this->_env);
    in >> this->_working_directory >> this->_error >> this->_application;
}

void sbnd::application_kernel::act() {
    ::sbn::application app(arguments(), environment());
    app.working_directory(working_directory());
    app.set_credentials(credentials().uid, credentials().gid);
    app.make_master();
    try {
        application(app.id());
        factory.process().add(app);
        this->return_to_parent(sbn::exit_code::success);
        factory.unix().send(this);
    } catch (const sys::bad_call& err) {
        set_error(err.what());
        this->return_to_parent(sbn::exit_code::error);
        factory.unix().send(this);
        this->log("execute error _,app=_", err, app.id());
    } catch (const sbn::error& err) {
        set_error(err.what());
        this->return_to_parent(sbn::exit_code::error);
        factory.unix().send(this);
        this->log("execute error _,app=_", err, app.id());
    } catch (const std::exception& err) {
        set_error(err.what());
        this->return_to_parent(sbn::exit_code::error);
        factory.unix().send(this);
        this->log("execute error _,app=_", err.what(), app.id());
    } catch (...) {
        set_error("unknown error");
        this->return_to_parent(sbn::exit_code::error);
        factory.unix().send(this);
        this->log("execute error _", "<unknown>");
    }
}
