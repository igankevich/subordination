#include <zlib.h>

#include <cmath>
#include <sstream>

#include <subordination/api.hh>
#include <subordination/guile/kernel.hh>
#include <subordination/guile/macros.hh>

namespace {

    SCM type = SCM_UNDEFINED;
    SCM kernel_buffer_type = SCM_UNDEFINED;
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
    std::mutex first_node_mutex;

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
        SCM pipeline = sym_local, child_pipeline = sym_local, block_size = SCM_UNDEFINED;
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

    sbn::kernel_buffer* to_kernel_buffer_ptr(SCM scm) {
        return reinterpret_cast<sbn::kernel_buffer*>(scm_foreign_object_ref(scm, 0));
    }

    void finalize_kernel_buffer_ptr(SCM scm) {
        //to_kernel_buffer_ptr(scm);
    }

    SCM make_kernel_buffer_ptr(sbn::kernel_buffer& buffer) {
        return scm_make_foreign_object_1(::kernel_buffer_type, &buffer);
    }

    enum class Types: sys::u16 {
        Unknown=0,
        Integer=1,
        Real=2,
        Complex=3,
        String=4,
        Symbol=5,
        Pair=6,
        List=7,
        Boolean=8,
        Keyword=9,
        Undefined=10,
        Character=11,
        User_defined=12,
    };

    SCM s_object_write(SCM s_buffer, SCM object) {
        using namespace sbn::guile;
        auto buffer = to_kernel_buffer_ptr(s_buffer);
        object_write(*buffer, object);
        return SCM_UNSPECIFIED;
    }

    SCM s_object_read(SCM s_buffer) {
        using namespace sbn::guile;
        auto buffer = to_kernel_buffer_ptr(s_buffer);
        SCM result = SCM_UNSPECIFIED;
        object_read(*buffer, result);
        return result;
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
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "kernel-base::react _", this->_num_children);
    #endif
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
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "act _", object_to_string(code));
    #endif
    scm_call(scm_eval(code, scm_current_module()),
             kernel_weak_ptr_make(this_ptr()),
             this->_data,
             SCM_UNDEFINED);
}

void sbn::guile::Kernel::react(sbn::kernel_ptr&& child) {
    SCM code = scm_list_3(scm_sym_lambda,
                          scm_list_n(sym_self, sym_child, sym_data, SCM_UNDEFINED),
                          this->_react);
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "react _", object_to_string(code));
    #endif
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
    ::kernel_buffer_type = scm_make_foreign_object_type(
        scm_from_utf8_symbol("<kernel-buffer>"),
        scm_list_1(scm_from_utf8_symbol("kernel-buffer")),
        ::finalize_kernel_buffer_ptr);
    scm_c_define("<kernel-buffer>", ::kernel_buffer_type);
    scm_c_export("<kernel-buffer>", nullptr);
    define_procedure<0,0,3>(
        "make-kernel",
        {}, {}, {"act","react","data"},
R"(Make kernel with the specified @var{act} and @var{react} procedures.
The procedures are supplied in unevaluated form.)",
        SBN_GUILE_1(kernel_make));
    define_procedure<1,1,0>(
        "send",
        {"kernel"}, {"pipeline"}, {},
R"(Send @var{kernel} to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        SBN_GUILE_2(kernel_send));
    define_procedure<2,1,0>(
        "upstream",
        {"parent","child"}, {"pipeline"}, {},
R"(Make @var{child} a child of @var{parent} and send it to the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        SBN_GUILE_3(kernel_upstream));
    define_procedure<1,2,0>(
        "commit",
        {"kernel"}, {"exit-code","pipeline"}, {},
R"(Send @var{kernel} to its parent with @var{exit-code} via the @var{pipeline}.
Pipeline can be either @code{local} or @code{remote}.
The default is pipeline is @code{local}.)",
        SBN_GUILE_3(kernel_commit));
    define_procedure<1,0,0>(
        "kernel-data",
        {"kernel"}, {}, {},
        R"(Get @var{kernel} state.)",
        SBN_GUILE_1(kernel_data));
    define_procedure<1,0,0>(
        "kernel-application-arguments",
        {"kernel"}, {}, {},
        R"(Get @var{kernel} command line arguments.)",
        SBN_GUILE_1(kernel_application_arguments));
    define_procedure<1,0,0>(
        "gzip-file->string",
        {"path"}, {}, {},
        R"(Read GZ file as string.)",
        SBN_GUILE_1(gzip_file_to_string));
    define_procedure<6,0,0>(
        "compute-variance",
        {"length","alpha1","alpha2","r1","r2","density"}, {}, {},
        R"(Compute spectrum variance.)",
        SBN_GUILE_6(compute_variance<double>));
    define_procedure<2,0,3>(
        "kernel-map",
        {"proc","lists"}, {}, {"pipeline","child-pipeline","block-size"},
        R"(Parallel map.)",
        SBN_GUILE_3(kernel_map));
    define_procedure<3,0,0>(
        "kernel-react",
        {"cons","postamble","initial"}, {}, {},
        R"(Fold child kernels.)",
        SBN_GUILE_3(kernel_react));
    define_procedure<1,0,0>(
        "kernel-exit",
        {"exit-code"}, {}, {},
        R"(Exit the script.)",
        SBN_GUILE_1(kernel_exit));
    define_procedure<2,0,0>(
        "write-object",
        {"buffer","object"}, {}, {},
        R"(Write object to kernel byte buffer.)",
        SBN_GUILE_2(s_object_write));
    define_procedure<1,0,0>(
        "read-object",
        {"buffer"}, {}, {},
        R"(Read object from kernel byte buffer.)",
        SBN_GUILE_1(s_object_read));
    scm_c_define("*first-node*", scm_from_bool(false));
    scm_c_export("*first-node*", nullptr);
}

void sbn::guile::Main::act() {
    SCM ret = SCM_EOL;
    if (auto* a = target_application()) {
        const auto& args = a->arguments();
        const int nargs = args.size();
        for (int i=nargs-1; i>=1; --i) {
            ret = scm_cons(scm_from_utf8_string(args[i].data()), ret);
        }
        scm_c_define("*application-arguments*", ret);
        {
            std::lock_guard<std::mutex> lock(first_node_mutex);
            SCM first_node_var = scm_c_lookup("*first-node*");
            scm_variable_set_x(first_node_var, scm_from_bool(true));
        }
        set_current_kernel(this);
        if (nargs < 2) {
            throw std::invalid_argument("usage: sbn-guile script-name [script-args...]");
        }
        scm_c_primitive_load(a->arguments()[1].data());
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
    //auto* ppl = symbol_to_pipeline(this->_pipeline);
    std::vector<SCM> lists;
    for (SCM s=this->_lists; s!=SCM_EOL; s=scm_cdr(s)) {
        lists.emplace_back(scm_car(s));
    }
    int num_kernels = std::numeric_limits<int>::max();
    for (auto lst : lists) {
        auto len = scm_to_int(scm_length(lst));
        if (len < num_kernels) {
            num_kernels = len;
        }
    }
    int block_size = 1;
    if (is_bound(this->_block_size)) { block_size = scm_to_int(this->_block_size); }
    const auto last_block_size = block_size + num_kernels%block_size;
    num_kernels /= block_size;
    this->_num_kernels= num_kernels;
    for (int i=0; i<num_kernels; ++i) {
        const auto n = i==num_kernels-1 ? last_block_size : block_size;
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
        #if defined(SBN_DEBUG)
        sys::log_message("scm", "make-map-child-kernel _", object_to_string(child_lists));
        #endif
        auto child = make_pointer<Map_child_kernel>(this->_proc, child_lists, this->_pipeline);
        child->parent(this);
        sbn::factory.remote().send(std::move(child));
    }
}

void sbn::guile::Map_kernel::react(sbn::kernel_ptr&& child) {
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "map-kernel-react _", this->_num_kernels);
    #endif
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
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "kernel-base::read");
    #endif
    Kernel_base::read(in);
    object_read(in, this->_proc.get());
    object_read(in, this->_lists.get());
    object_read(in, this->_pipeline.get());
    object_read(in, this->_block_size.get());
    in.read(this->_num_kernels);
}
void sbn::guile::Map_kernel::write(sbn::kernel_buffer& out) const {
    #if defined(SBN_DEBUG)
    sys::log_message("scm", "kernel-base::write");
    #endif
    Kernel_base::write(out);
    object_write(out, this->_proc);
    object_write(out, this->_lists);
    object_write(out, this->_pipeline);
    object_write(out, this->_block_size);
    out.write(this->_num_kernels);
}

void sbn::guile::Map_child_kernel::act() {
    SCM code = scm_eval(this->_proc, scm_current_module());
    SCM map = scm_variable_ref(scm_c_lookup("map"));
    result(scm_apply_0(map, scm_cons(code, this->_lists)));
    //auto* ppl = symbol_to_pipeline(this->_pipeline);
    return_to_parent();
    sbn::factory.remote().send(std::move(this_ptr()));
}

void sbn::guile::Map_child_kernel::read(sbn::kernel_buffer& in) {
    Kernel_base::read(in);
    object_read(in, this->_proc);
    object_read(in, this->_lists);
    object_read(in, this->_pipeline);
}

void sbn::guile::Map_child_kernel::write(sbn::kernel_buffer& out) const {
    Kernel_base::write(out);
    object_write(out, this->_proc);
    object_write(out, this->_lists);
    object_write(out, this->_pipeline);
}

void sbn::guile::Kernel_base::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    {
        std::lock_guard<std::mutex> lock(first_node_mutex);
        SCM first_node_var = scm_c_lookup("*first-node*");
        if (!scm_is_true(scm_variable_ref(first_node_var))) {
            //scm_c_eval_string("(use-modules (ice-9 format)) (format (current-error-port) \"ARGUMENTS: ~a\n\" (car (cdr *global-arguments*)))\n(force-output (current-error-port))");
            SCM global_arguments = scm_variable_ref(scm_c_lookup("*global-arguments*"));
            scm_primitive_load(scm_cadr(global_arguments));
            scm_variable_set_x(first_node_var, scm_from_bool(true));
        }
    }
    in.read(this->_num_children);
    in.read(this->_no_children);
    object_read(in, this->_result);
    object_read(in, this->_react);
    object_read(in, this->_postamble);
}

void sbn::guile::Kernel_base::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out.write(this->_num_children);
    out.write(this->_no_children);
    object_write(out, this->_result);
    object_write(out, this->_react);
    object_write(out, this->_postamble);
}

void sbn::guile::Main::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    if (in.remaining() == 0) { return; }
    in.read(this->_num_children);
    in.read(this->_no_children);
}

void sbn::guile::Main::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out.write(this->_num_children);
    out.write(this->_no_children);
}

void sbn::guile::object_write(sbn::kernel_buffer& buffer, SCM object) {
    using namespace sbn::guile;
    auto name = scm_class_name(scm_class_of(object));
    if (!is_bound(object)) {
        buffer.write(Types::Undefined);
    } else if (symbol_equal(name,scm_from_utf8_symbol("<integer>"))) {
        buffer.write(Types::Integer);
        buffer.write(scm_to_uint64(object));
    } else if (symbol_equal(name,scm_from_utf8_symbol("<real>"))) {
        buffer.write(Types::Real);
        buffer.write(scm_to_double(object));
    } else if (symbol_equal(name,scm_from_utf8_symbol("<complex>"))) {
        buffer.write(Types::Complex);
        buffer.write(scm_c_real_part(object));
        buffer.write(scm_c_imag_part(object));
    } else if (symbol_equal(name,scm_from_utf8_symbol("<boolean>"))) {
        buffer.write(Types::Boolean);
        buffer.write(scm_is_true(object));
    } else if (symbol_equal(name,scm_from_utf8_symbol("<unknown>"))) {
        buffer.write(Types::Unknown);
    } else if (scm_is_true(scm_char_p(object))) {
        buffer.write(Types::Character);
        buffer.write(scm_to_char(scm_char_to_integer(object)));
    } else if (scm_is_true(scm_string_p(object))) {
        buffer.write(Types::String);
        c_string ptr(scm_to_utf8_string(object));
        sys::u64 n = std::strlen(ptr.get());
        buffer.write(n);
        buffer.write(ptr.get(), n);
    } else if (scm_is_true(scm_symbol_p(object))) {
        buffer.write(Types::Symbol);
        c_string ptr(scm_to_utf8_string(scm_symbol_to_string(object)));
        sys::u64 n = std::strlen(ptr.get());
        buffer.write(n);
        buffer.write(ptr.get(), n);
    } else if (scm_is_true(scm_keyword_p(object))) {
        buffer.write(Types::Keyword);
        c_string ptr(scm_to_utf8_string(scm_symbol_to_string(scm_keyword_to_symbol(object))));
        sys::u64 n = std::strlen(ptr.get());
        buffer.write(n);
        buffer.write(ptr.get(), n);
    } else if (scm_is_true(scm_list_p(object))) {
        buffer.write(Types::List);
        buffer.write(scm_to_uint64(scm_length(object)));
        for (; object != SCM_EOL; object = scm_cdr(object)) {
            object_write(buffer, scm_car(object));
        }
    } else if (scm_is_true(scm_pair_p(object))) {
        buffer.write(Types::Pair);
        object_write(buffer, scm_car(object));
        object_write(buffer, scm_cdr(object));
    } else {
        SCM write_name = scm_string_append(
            scm_list_2(scm_symbol_to_string(name), scm_from_utf8_string("-write")));
        SCM write = scm_variable_ref(scm_lookup(scm_string_to_symbol(write_name)));
        c_string name_str(scm_to_utf8_string(scm_symbol_to_string(name)));
        if (!is_bound(write)) {
            std::stringstream tmp;
            tmp << "Unsupported type: " << name_str.get();
            throw std::invalid_argument(tmp.str());
        }
        buffer.write(Types::User_defined);
        sys::u64 n = std::strlen(name_str.get());
        buffer.write(n);
        buffer.write(name_str.get(), n);
        scm_apply_0(write, scm_list_2(make_kernel_buffer_ptr(buffer), object));
    }
}

void sbn::guile::object_read(sbn::kernel_buffer& buffer, SCM& result) {
    using namespace sbn::guile;
    Types type{};
    buffer.read(type);
    if (type == Types::Unknown) {
        result = SCM_UNSPECIFIED;
    } else if (type == Types::Undefined) {
        result = SCM_UNDEFINED;
    } else if (type == Types::Integer) {
        sys::u64 tmp{};
        buffer.read(tmp);
        result = scm_from_uint64(tmp);
    } else if (type == Types::Real) {
        double tmp{};
        buffer.read(tmp);
        result = scm_from_double(tmp);
    } else if (type == Types::Complex) {
        double a{}, b{};
        buffer.read(a);
        buffer.read(b);
        result = scm_c_make_rectangular(a,b);
    } else if (type == Types::Boolean) {
        bool a{};
        buffer.read(a);
        result = scm_from_bool(a);
    } else if (type == Types::String) {
        std::string s;
        buffer.read(s);
        result = scm_from_utf8_string(s.data());
    } else if (type == Types::Symbol) {
        std::string s;
        buffer.read(s);
        result = scm_from_utf8_symbol(s.data());
    } else if (type == Types::Keyword) {
        std::string s;
        buffer.read(s);
        result = scm_from_utf8_keyword(s.data());
    } else if (type == Types::Character) {
        char ch{};
        buffer.read(ch);
        result = scm_integer_to_char(scm_from_char(ch));
    } else if (type == Types::Pair) {
        SCM a = SCM_UNSPECIFIED, b = SCM_UNSPECIFIED;
        object_read(buffer, a);
        object_read(buffer, b);
        result = scm_cons(a,b);
    } else if (type == Types::List) {
        sys::u64 n{};
        buffer.read(n);
        result = SCM_EOL;
        for (sys::u64 i=0; i<n; ++i) {
            SCM x = SCM_UNSPECIFIED;
            object_read(buffer, x);
            result = scm_cons(x, result);
        }
        result = scm_reverse(result);
    } else if (type == Types::User_defined) {
        std::string s;
        buffer.read(s);
        SCM type_name = scm_from_utf8_symbol(s.data());
        SCM type = scm_variable_ref(scm_lookup(type_name));
        SCM object = scm_make(scm_list_1(type));
        SCM read_name = scm_string_append(
            scm_list_2(scm_symbol_to_string(type_name), scm_from_utf8_string("-read")));
        SCM read = scm_variable_ref(scm_lookup(scm_string_to_symbol(read_name)));
        c_string name_str(scm_to_utf8_string(scm_symbol_to_string(type_name)));
        if (!is_bound(read)) {
            std::stringstream tmp;
            tmp << "Unsupported type: " << name_str.get();
            throw std::invalid_argument(tmp.str());
        }
        scm_apply_0(read, scm_list_2(make_kernel_buffer_ptr(buffer), object));
        result = object;
    } else {
        std::stringstream tmp;
        tmp << "Unsupported type: " << sys::u64(type);
        throw std::invalid_argument(tmp.str());
    }
}
