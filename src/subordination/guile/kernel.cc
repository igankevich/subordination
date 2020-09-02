#include <subordination/api.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/guile/kernel.hh>
#include <subordination/guile/macros.hh>

namespace {

    SCM type = SCM_UNDEFINED;
    SCM weak_ptr_type = SCM_UNDEFINED;
    SCM sym_self = SCM_UNDEFINED;
    SCM sym_parent = SCM_UNDEFINED;
    SCM sym_child = SCM_UNDEFINED;
    SCM sym_data = SCM_UNDEFINED;
    SCM sym_local = SCM_UNDEFINED;
    SCM sym_remote = SCM_UNDEFINED;
    SCM kw_act = SCM_UNDEFINED;
    SCM kw_react = SCM_UNDEFINED;
    SCM kw_data = SCM_UNDEFINED;

    sbn::kernel_ptr& to_kernel_ptr(SCM scm) {
        return *reinterpret_cast<sbn::kernel_ptr*>(scm_foreign_object_ref(scm, 0));
    }

    sbn::kernel_ptr* to_kernel_weak_ptr(SCM scm) {
        return *reinterpret_cast<sbn::kernel_ptr**>(scm_foreign_object_ref(scm, 0));
    }

    sbn::kernel_ptr& to_kernel_ptr_any(SCM scm) {
        SCM t = scm_class_of(scm);
        if (t == ::type) {
            return *reinterpret_cast<sbn::kernel_ptr*>(scm_foreign_object_ref(scm, 0));
        }
        if (t == ::weak_ptr_type) {
            return **reinterpret_cast<sbn::kernel_ptr**>(scm_foreign_object_ref(scm, 0));
        }
        throw std::runtime_error("bad guile kernel type");
    }

    void finalize_kernel_ptr(SCM scm) {
        to_kernel_ptr(scm).reset();
    }

    SCM kernel_make_impl(sbn::kernel_ptr&& kernel) {
        using namespace sbn::guile;
        auto ptr = construct<sbn::kernel_ptr>("kernel", std::move(kernel));
        return scm_make_foreign_object_1(::type, ptr);
    }

    SCM kernel_make(SCM rest) {
        using namespace sbn::guile;
        SCM act = SCM_UNSPECIFIED, react = SCM_UNSPECIFIED, data = SCM_UNSPECIFIED;
        scm_c_bind_keyword_arguments("make-kernel", rest,
                                     scm_t_keyword_arguments_flags{},
                                     kw_act, &act,
                                     kw_react, &react,
                                     kw_data, &data,
                                     SCM_UNDEFINED);
        return kernel_make_impl(sbn::make_pointer<Kernel>(act, react, data));
    }

    SCM kernel_weak_ptr_make(sbn::kernel_ptr& kernel) {
        using namespace sbn::guile;
        auto ptr = construct<sbn::kernel_ptr*>("kernel", &kernel);
        return scm_make_foreign_object_1(::weak_ptr_type, ptr);
    }

    sbn::pipeline* symbol_to_pipeline(SCM s) {
        using namespace sbn::guile;
        if (is_bound(s) && symbol_equal(s, sym_remote)) {
            return &sbn::factory.remote();
        }
        return &sbn::factory.local();
    }

    SCM kernel_send(SCM kernel, SCM pipeline) {
        symbol_to_pipeline(pipeline)->send(std::move(to_kernel_ptr(kernel)));
        return SCM_UNSPECIFIED;
    }

    SCM kernel_upstream(SCM self, SCM child, SCM pipeline) {
        using namespace sbn::guile;
        auto& kernel = to_kernel_ptr(child);
        kernel->parent(to_kernel_weak_ptr(self)->get());
        symbol_to_pipeline(pipeline)->send(std::move(kernel));
        return SCM_UNSPECIFIED;
    }

    SCM kernel_commit(SCM self, SCM exit_code, SCM pipeline) {
        auto& kernel = *to_kernel_weak_ptr(self);
        sbn::exit_code code = kernel->return_code();
        if (sbn::guile::is_bound(exit_code)) {
            code = sbn::exit_code(scm_to_int(exit_code));
        } else if (kernel->return_code() == sbn::exit_code::undefined) {
            code = sbn::exit_code::success;
        }
        kernel->return_to_parent(code);
        symbol_to_pipeline(pipeline)->send(std::move(kernel));
        return SCM_UNSPECIFIED;
    }

    SCM kernel_data(SCM self) {
        using namespace sbn::guile;
        return reinterpret_cast<Kernel*>(to_kernel_ptr_any(self).get())->data();
    }

}

void sbn::guile::Kernel::act() {
    scm_init_guile();
    SCM code = scm_list_3(scm_sym_lambda,
                          scm_list_n(sym_self, sym_data, SCM_UNDEFINED),
                          this->_act);
    sys::log_message("scm", "act _", object_to_string(code));
    scm_call(scm_eval(code, scm_current_module()),
             kernel_weak_ptr_make(this_ptr()),
             this->_data,
             SCM_UNDEFINED);
}

void sbn::guile::Kernel::react(sbn::kernel_ptr&& child) {
    scm_init_guile();
    SCM code = scm_list_3(scm_sym_lambda,
                          scm_list_n(sym_self, sym_child, sym_data, SCM_UNDEFINED),
                          this->_react);
    sys::log_message("scm", "react _", object_to_string(code));
    scm_call(scm_eval(code, scm_current_module()),
             kernel_weak_ptr_make(this_ptr()),
             kernel_make_impl(std::move(child)),
             this->_data,
             SCM_UNDEFINED);
}

void sbn::guile::Kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    std::string tmp;
    in.read(tmp);
    this->_act = string_to_object(tmp);
    tmp.clear();
    in.read(tmp);
    this->_react = string_to_object(tmp);
    tmp.clear();
    in.read(tmp);
    this->_data = string_to_object(tmp);
}

void sbn::guile::Kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out.write(object_to_string(this->_act));
    out.write(object_to_string(this->_react));
    out.write(object_to_string(this->_data));
}

void sbn::guile::kernel_define() {
    ::sym_self = scm_from_utf8_symbol("*self*");
    ::sym_parent = scm_from_utf8_symbol("*parent*");
    ::sym_child = scm_from_utf8_symbol("*child*");
    ::sym_data = scm_from_utf8_symbol("*data*");
    ::sym_local = scm_from_utf8_symbol("local");
    ::sym_remote = scm_from_utf8_symbol("remote");
    ::kw_act = scm_from_utf8_keyword("act");
    ::kw_react = scm_from_utf8_keyword("react");
    ::kw_data = scm_from_utf8_keyword("data");
    ::type = scm_make_foreign_object_type(
        scm_from_utf8_symbol("<kernel>"),
        scm_list_1(scm_from_utf8_symbol("kernel")),
        ::finalize_kernel_ptr);
    scm_c_define("<kernel>", ::type);
    scm_c_export("<kernel>", nullptr);
    ::weak_ptr_type = scm_make_foreign_object_type(
        scm_from_utf8_symbol("<kernel-weak-ptr>"),
        scm_list_1(scm_from_utf8_symbol("kernel-weak-ptr")),
        nullptr);
    scm_c_define("<kernel-weak-ptr>", ::weak_ptr_type);
    scm_c_export("<kernel-weak-ptr>", nullptr);
    define_procedure<0,0,3>(
        "make-kernel",
        {}, {}, {"act","react","data"},
R"("Make kernel with the specified @var{act} and @var{react} procedures.
The procedures are supplied in unevaluated form.)",
        VTB_GUILE_1(kernel_make));
    define_procedure<1,1,0>(
        "send",
        {"kernel"}, {"pipeline"}, {},
R"("Send @var{kernel} to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_2(kernel_send));
    define_procedure<2,1,0>(
        "upstream",
        {"parent","child"}, {"pipeline"}, {},
R"("Make @var{child} a child of @var{parent} and send it to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_3(kernel_upstream));
    define_procedure<1,2,0>(
        "commit",
        {"kernel"}, {"exit-code","pipeline"}, {},
R"("Send @var{kernel} to its parent with @var{exit-code} via the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_3(kernel_commit));
    define_procedure<1,0,0>(
        "kernel-data",
        {"kernel"}, {}, {},
        R"("Get @var{kernel} state.)",
        VTB_GUILE_1(kernel_data));
}

void sbn::guile::Main::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    if (in.remaining() == 0) {
        if (auto* a = target_application()) {
            const auto& args = a->arguments();
            if (!args.empty()) { load_main_kernel(args.front().data()); }
        }
    }
}

void sbn::guile::Main::load_main_kernel(const char* path) {
    scm_init_guile();
    SCM main_kernel = scm_c_primitive_load(path);
    auto k = sbn::pointer_dynamic_cast<Kernel>(std::move(to_kernel_ptr(main_kernel)));
    *this = *k;
}
