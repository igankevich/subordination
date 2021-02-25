#include <zlib.h>

#include <cmath>
#include <sstream>

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
    SCM kw_pipeline = SCM_UNDEFINED;
    SCM kw_child_pipeline = SCM_UNDEFINED;
    SCM kw_block_size = SCM_UNDEFINED;
    SCM fluid_current_kernel = SCM_UNDEFINED;

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

    SCM kernel_application_arguments(SCM self) {
        using namespace sbn::guile;
        auto* k = reinterpret_cast<Kernel*>(to_kernel_ptr_any(self).get());
        SCM ret = SCM_EOL;
        if (auto* a = k->target_application()) {
            const auto& args = a->arguments();
            const int nargs = args.size();
            for (int i=nargs-1; i>=0; --i) {
                ret = scm_cons(scm_from_utf8_string(args[i].data()), ret);
            }
        }
        return ret;
    }

    void unprotect(SCM s) {
        if (s == SCM_UNSPECIFIED || s == SCM_UNDEFINED) { return; }
        scm_gc_unprotect_object(s);
    }

    SCM gzip_file_to_string(SCM path) {
        using namespace sbn::guile;
        auto file = ::gzopen(to_c_string(path).get(), "rb");
        char buf[4096];
        std::stringstream tmp;
        int count = 0;
        while ((count=::gzread(file, buf, sizeof(buf))) != 0) {
            tmp.write(buf, count);
        }
        ::gzclose(file);
        return scm_from_utf8_string(tmp.str().data());
    }

    template <class T>
    SCM compute_variance(SCM length, SCM alpha1, SCM alpha2, SCM rr1, SCM rr2, SCM density) {
        const auto n = scm_to_int(length);
        const T theta0 = 0;
        const T theta1 = 2.0f*M_PI;
        //int32_t n = std::min(this->_frequencies.size(), this->_data[0].size());
        T sum = 0;
        for (int i=0; i<n; ++i) {
            auto a1 = scm_to_double(scm_car(alpha1));
            auto a2 = scm_to_double(scm_car(alpha2));
            auto r1 = scm_to_double(scm_car(rr1));
            auto r2 = scm_to_double(scm_car(rr2));
            auto d = scm_to_double(scm_car(density));
            for (int j=0; j<n; ++j) {
                const T theta = theta0 + (theta1 - theta0)*j/n;
                sum += d * (T{1}/T{M_PI}) *
                    (T{0.5} + T{0.01}*r1*std::cos(      theta - a1)
                            + T{0.01}*r2*std::cos(T{2}*(theta - a2)));
            }
            alpha1 = scm_cdr(alpha1);
            alpha2 = scm_cdr(alpha2);
            rr1 = scm_cdr(rr1);
            rr2 = scm_cdr(rr2);
            density = scm_cdr(density);
        }
        return scm_from_double(sum);
    }

    void set_current_kernel(sbn::guile::Kernel_base* k) {
        scm_fluid_set_x(fluid_current_kernel, scm_from_pointer(k, nullptr));
    }

    sbn::guile::Kernel_base* get_current_kernel() {
        auto ptr = scm_to_pointer(scm_fluid_ref(fluid_current_kernel));
        return reinterpret_cast<sbn::guile::Kernel_base*>(ptr);
    }

    SCM kernel_map(SCM proc, SCM lists, SCM rest) {
        using namespace sbn::guile;
        SCM pipeline = sym_local, child_pipeline = sym_local,
           block_size = SCM_UNDEFINED;
        scm_c_bind_keyword_arguments("kernel-map", rest,
                                     scm_t_keyword_arguments_flags{},
                                     kw_pipeline, &pipeline,
                                     kw_child_pipeline, &child_pipeline,
                                     kw_block_size, &block_size,
                                     SCM_UNDEFINED);
        auto* parent = get_current_kernel();
        auto child = sbn::make_pointer<Map_kernel>(proc, scm_list_1(lists),
                                                   child_pipeline, block_size);
        parent->upstream(std::move(child), symbol_to_pipeline(pipeline));
        return SCM_UNSPECIFIED;
    }

    SCM kernel_react(SCM proc, SCM postamble, SCM initial) {
        auto* parent = get_current_kernel();
        parent->react(proc);
        parent->postamble(postamble);
        parent->result(initial);
        parent->upstream_children();
        return SCM_UNSPECIFIED;
    }

    SCM kernel_exit(SCM exit_code) {
        sbn::exit(scm_to_int(exit_code));
        return SCM_UNSPECIFIED;
    }

}

void sbn::guile::Kernel_base::upstream(sbn::kernel_ptr&& child, pipeline* ppl) {
    child->parent(this);
    this->_children.emplace_back(std::move(child), ppl);
}

void sbn::guile::Kernel_base::upstream_children() {
    this->_num_children = this->_children.size();
    this->_no_children = this->_children.empty();
    for (auto& pair : this->_children) {
        pair.second->send(std::move(pair.first));
    }
    this->_children.clear();
    this->_children.shrink_to_fit();
}

void sbn::guile::Kernel_base::react(sbn::kernel_ptr&& child) {
    --this->_num_children;
    auto k = sbn::pointer_dynamic_cast<Kernel_base>(std::move(child));
    result(scm_call(scm_eval(this->_react, scm_current_module()),
                    k->result(),
                    result(),
                    SCM_UNDEFINED));
    if (this->_num_children == 0) {
        scm_call(scm_eval(this->_postamble, scm_current_module()),
                 result(),
                 SCM_UNDEFINED);
        auto* ppl = source_pipeline() ? source_pipeline() : &sbn::factory.local();
        return_to_parent();
        ppl->send(std::move(this_ptr()));
    }
}

sbn::guile::Kernel::~Kernel() noexcept {
    unprotect(this->_act);
    unprotect(this->_react);
    unprotect(this->_data);
}

void sbn::guile::Kernel::act() {
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
    ::kw_pipeline = scm_from_utf8_keyword("pipeline");
    ::kw_child_pipeline = scm_from_utf8_keyword("child-pipeline");
    ::kw_block_size = scm_from_utf8_keyword("block-size-pipeline");
    ::fluid_current_kernel = scm_make_fluid_with_default(scm_from_pointer(nullptr,nullptr));
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
    define_procedure<1,0,0>(
        "kernel-application-arguments",
        {"kernel"}, {}, {},
        R"("Get @var{kernel} command line arguments.)",
        VTB_GUILE_1(kernel_application_arguments));
    define_procedure<1,0,0>(
        "gzip-file->string",
        {"path"}, {}, {},
        R"("Read GZ file as string.)",
        VTB_GUILE_1(gzip_file_to_string));
    define_procedure<6,0,0>(
        "compute-variance",
        {"length","alpha1","alpha2","r1","r2","density"}, {}, {},
        R"("Compute spectrum variance.)",
        VTB_GUILE_6(compute_variance<double>));
    define_procedure<2,3,0>(
        "kernel-map",
        {"proc","lists"}, {"pipeline","child-pipeline","block-size"}, {},
        R"("Parallel map.)",
        VTB_GUILE_3(kernel_map));
    define_procedure<3,0,0>(
        "kernel-react",
        {"cons","postamble","initial"}, {}, {},
        R"("Fold child kernels.)",
        VTB_GUILE_3(kernel_react));
    define_procedure<1,0,0>(
        "kernel-exit",
        {"exit-code"}, {}, {},
        R"("Exit the script.)",
        VTB_GUILE_1(kernel_exit));
}

void sbn::guile::Main::act() {
    SCM ret = SCM_EOL;
    if (auto* a = target_application()) {
        const auto& args = a->arguments();
        const int nargs = args.size();
        for (int i=nargs-1; i>=0; --i) {
            ret = scm_cons(scm_from_utf8_string(args[i].data()), ret);
        }
        scm_c_define("*application-arguments*", ret);
        set_current_kernel(this);
        scm_c_primitive_load(a->arguments().front().data());
    }
    if (no_children()) {
        auto* ppl = source_pipeline() ? source_pipeline() : &sbn::factory.local();
        if (!parent()) {
            sbn::exit(0);
        } else {
            return_to_parent();
            ppl->send(std::move(this_ptr()));
        }
    }
}

void sbn::guile::Map_kernel::act() {
    result(SCM_EOL);
    auto* ppl = symbol_to_pipeline(this->_pipeline);
    std::vector<SCM> lists;
    for (SCM s=this->_lists; s!=SCM_EOL; s=scm_cdr(s)) {
        lists.emplace_back(scm_car(s));
    }
    this->_num_kernels = std::numeric_limits<int>::max();
    for (auto lst : lists) {
        auto len = scm_to_int(scm_length(lst));
        if (len < this->_num_kernels) {
            this->_num_kernels = len;
        }
    }
    int block_size = 1;
    if (is_bound(this->_block_size)) { block_size = scm_to_int(this->_block_size); }
    const auto last_block_size = block_size + this->_num_kernels%block_size;
    this->_num_kernels /= block_size;
    for (int i=0; i<this->_num_kernels; ++i) {
        const auto n = i==this->_num_kernels-1 ? last_block_size : block_size;
        SCM child_lists = SCM_EOL;
        for (auto& parent_list : lists) {
            SCM lst = SCM_EOL;
            for (int j=0; j<n; ++j) {
                lst = scm_cons(scm_car(parent_list), lst);
                parent_list = scm_cdr(parent_list);
            }
            lst = scm_reverse(lst);
            child_lists = scm_cons(lst, child_lists);
        }
        child_lists = scm_reverse(child_lists);
        auto child = make_pointer<Map_child_kernel>(this->_proc, child_lists, this->_pipeline);
        child->parent(this);
        ppl->send(std::move(child));
    }
}

void sbn::guile::Map_kernel::react(sbn::kernel_ptr&& child) {
    sys::log_message("scm", "map-kernel-react _", this->_num_kernels);
    auto k = sbn::pointer_dynamic_cast<Kernel_base>(std::move(child));
    result(scm_cons(k->result(), result()));
    --this->_num_kernels;
    if (this->_num_kernels == 0) {
        //scm_write(result(), scm_current_error_port());
        //scm_flush_all_ports();
        auto* ppl = source_pipeline() ? source_pipeline() : &sbn::factory.local();
        return_to_parent();
        ppl->send(std::move(this_ptr()));
    }
}

void sbn::guile::Map_kernel::read(sbn::kernel_buffer& in) {
}
void sbn::guile::Map_kernel::write(sbn::kernel_buffer& out) const {
}

void sbn::guile::Map_child_kernel::act() {
    SCM code = scm_eval(this->_proc, scm_current_module());
    SCM map = scm_variable_ref(scm_c_lookup("map"));
    result(scm_apply_0(map, scm_cons(code, this->_lists)));
    auto* ppl = symbol_to_pipeline(this->_pipeline);
    return_to_parent();
    ppl->send(std::move(this_ptr()));
}

void sbn::guile::Map_child_kernel::read(sbn::kernel_buffer& in) {
}
void sbn::guile::Map_child_kernel::write(sbn::kernel_buffer& out) const {
}
