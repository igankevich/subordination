#include <subordination/api.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/guile/kernel.hh>
#include <subordination/guile/macros.hh>

namespace {

    SCM type = SCM_UNDEFINED;
    SCM sym_self = SCM_UNDEFINED;
    SCM sym_parent = SCM_UNDEFINED;
    SCM sym_local = SCM_UNDEFINED;
    SCM sym_remote = SCM_UNDEFINED;

    sbn::kernel_ptr& to_kernel_ptr(SCM scm) {
        return *reinterpret_cast<sbn::kernel_ptr*>(scm_foreign_object_ref(scm, 0));
    }

    void finalize_kernel_ptr(SCM scm) {
        to_kernel_ptr(scm).reset();
    }

    SCM kernel_make(SCM act, SCM react) {
        using namespace sbn::guile;
        auto ptr = construct<sbn::kernel_ptr>("kernel", sbn::make_pointer<Kernel>(act, react));
        return scm_make_foreign_object_1(::type, ptr);
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

    SCM kernel_upstream(SCM child, SCM pipeline) {
        using namespace sbn::guile;
        auto& kernel = to_kernel_ptr(child);
        SCM self = scm_variable_ref(scm_lookup(sym_self));
        kernel->parent(reinterpret_cast<sbn::kernel_ptr*>(self)->get());
        symbol_to_pipeline(pipeline)->send(std::move(kernel));
        return SCM_UNSPECIFIED;
    }

    SCM kernel_commit(SCM exit_code, SCM pipeline) {
        SCM self = scm_variable_ref(scm_lookup(sym_self));
        auto& kernel = *reinterpret_cast<sbn::kernel_ptr*>(self);
        kernel->return_to_parent(sbn::exit_code(scm_to_int(exit_code)));
        symbol_to_pipeline(pipeline)->send(std::move(kernel));
        return SCM_UNSPECIFIED;
    }

}

void sbn::guile::Kernel::act() {
    scm_eval(
        scm_list_3(
            scm_sym_let,
            scm_list_n(
                scm_list_2(sym_self, reinterpret_cast<SCM>(&this_ptr())),
                scm_list_2(sym_parent, reinterpret_cast<SCM>(parent())),
                SCM_UNDEFINED),
            this->_act),
        scm_current_module());
}

void sbn::guile::Kernel::react(sbn::kernel_ptr&& child) {
    scm_eval(this->_react, scm_current_module());
}

void sbn::guile::Kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    std::string tmp;
    in.read(tmp);
    this->_act = string_to_object(tmp);
    tmp.clear();
    in.read(tmp);
    this->_react = string_to_object(tmp);
}

void sbn::guile::Kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    out.write(object_to_string(this->_act));
    out.write(object_to_string(this->_react));
}

void sbn::guile::kernel_define() {
    ::sym_self = scm_from_utf8_symbol("*self*");
    ::sym_parent = scm_from_utf8_symbol("*parent*");
    ::sym_local = scm_from_utf8_symbol("local");
    ::sym_remote = scm_from_utf8_symbol("remote");
    ::type = scm_make_foreign_object_type(
        scm_from_utf8_symbol("<kernel>"),
        scm_list_1(scm_from_utf8_symbol("kernel")),
        ::finalize_kernel_ptr);
    scm_c_define("<kernel>", ::type);
    scm_c_export("<kernel>", nullptr);
    define_procedure<2,0,0>(
        "make-kernel",
        {"act","react"}, {}, {},
R"("Make kernel with the specified @var{act} and @var{react} procedures.
The procedures are supplied in unevaluated form.)",
        VTB_GUILE_2(kernel_make));
    define_procedure<1,1,0>(
        "send",
        {"kernel"}, {"pipeline"}, {},
R"("Send @var{kernel} to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_2(kernel_send));
    define_procedure<1,1,0>(
        "upstream",
        {"kernel"}, {"pipeline"}, {},
R"("Make @var{kernel} a child of @var{*self*} and send it to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_2(kernel_upstream));
    define_procedure<1,1,0>(
        "commit",
        {"exit-code"}, {"pipeline"}, {},
R"("Send @var{kernel} to its parent with @var{exit-code} via the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        VTB_GUILE_2(kernel_commit));
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
    SCM main_kernel = scm_c_primitive_load(path);
    auto k = sbn::pointer_dynamic_cast<Kernel>(std::move(to_kernel_ptr(main_kernel)));
    *this = *k;
}
