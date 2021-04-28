#include <subordination/guile/expression_kernel.hh>

#include <iostream>

#define SCM_DEBUG_VALUE(scm) {\
            SCM str3 = scm_open_output_string(); \
            scm_display(scm, str3); \
            std::string out3 = scm_to_locale_string(scm_get_output_string(str3)); \
            out3 += "\n"; \
            std::cout << out3 << std::endl; }

sbn::guile::expression_kernel_main::
expression_kernel_main(int argc, char* argv[]) {
    if (argc > 1) {
        this->_port = scm_open_file(
            scm_from_locale_string (argv[1]), 
            scm_from_locale_string ("r")
        );
        
    }
}

void 
sbn::guile::expression_kernel_main::act() {
    this->_scheme = scm_read(this->_port);
    sbn::upstream<sbn::Local>(
        this, 
        sbn::make_pointer<sbn::guile::expression_kernel>(this->_scheme, this->_definitions)
    );
}

void 
sbn::guile::expression_kernel_main::react(sbn::kernel_ptr&& child) {
    auto child_kernel = sbn::pointer_dynamic_cast<sbn::guile::expression_kernel>(std::move(child));
    for (const auto& pair: child_kernel->get_defs()) {
        this->_definitions[pair.first] = pair.second;
    }
    this->_scheme = scm_read(this->_port);
    if (!bool(scm_is_true(scm_eof_object_p(this->_scheme)))) {
        sbn::upstream<sbn::Local>(
            this, 
            sbn::make_pointer<sbn::guile::expression_kernel>(this->_scheme, this->_definitions)
        );
    } else {
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    }
}

void
sbn::guile::expression_kernel_main::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    //in >> this->_parent_arg;
    //in >> this->_scheme;
}

void
sbn::guile::expression_kernel_main::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    //out >> this->_parent_arg;
    //out >> this->_scheme;
}


void 
sbn::guile::expression_kernel::act() {
    bool is_list = bool(scm_is_true(scm_list_p(this->_scheme)));
    if (is_list) {
        bool check = bool(scm_is_true(scm_symbol_p(scm_car(this->_scheme))));
        SCM fun = scm_car(this->_scheme);
        if (check && bool(scm_is_true(scm_list_p(fun)))) {
            //wtf
            this->_result = this->_scheme;
            sbn::commit<sbn::Local>(std::move(this_ptr()));
        } else if (check && std::string("define") == scm_to_locale_string(scm_symbol_to_string(fun))) {
            auto head = scm_car(scm_cdr(this->_scheme));
            if (bool(scm_is_true(scm_list_p(head)))) {
                std::string key = scm_to_locale_string(scm_symbol_to_string(scm_car(head)));
                this->_definitions[key] = this->_scheme;
            } else {
                std::string key = scm_to_locale_string(scm_symbol_to_string(head));
                this->_definitions[key] = this->_scheme;
            }
            sbn::commit<sbn::Local>(std::move(this_ptr()));
        } else if (check && std::string("if") == scm_to_locale_string(scm_symbol_to_string(fun))) {
            sbn::upstream<sbn::Local>(this, sbn::make_pointer<sbn::guile::expression_kernel_if>(this->_scheme,this->_environment, this->_definitions));
        } else if (check && std::string("lambda") == scm_to_locale_string(scm_symbol_to_string(fun))) {
            this->_result = scm_eval(this->_scheme, this->_environment);
            sbn::commit<sbn::Local>(std::move(this_ptr()));
        } else if (check && std::string("quote") == scm_to_locale_string(scm_symbol_to_string(fun))) {
            this->_result = scm_car(scm_cdr(this->_scheme));
            sbn::commit<sbn::Local>(std::move(this_ptr()));
        } else {
            if (check && (this->_definitions.find(scm_to_locale_string(scm_symbol_to_string(fun))) != this->_definitions.end())) {
                std::string key = scm_to_locale_string(scm_symbol_to_string(fun));
                sbn::upstream<sbn::Local>(
                    this, 
                    sbn::make_pointer<sbn::guile::expression_kernel_define>(
                        this->_definitions[key], 
                        this->_environment,
                        this->_definitions,
                        scm_cdr(this->_scheme),
                        -1
                    )
                );
                this->_args.resize(1);
            } else {
                int args_number = 0;
                for (SCM s = this->_scheme; s != SCM_EOL; s = scm_cdr(s)) {
                    sbn::upstream<sbn::Local>(
                        this, 
                        sbn::make_pointer<sbn::guile::expression_kernel>(
                            scm_car(s), 
                            this->_environment,
                            this->_definitions, 
                            args_number
                        )
                    );
                    args_number++;
                }
                this->_args.resize(args_number);
            }
        }
    } else {
        bool check = bool(scm_is_true(scm_symbol_p(this->_scheme)));
        if (check && this->_definitions.find(scm_to_locale_string(scm_symbol_to_string(this->_scheme))) != this->_definitions.end()) {
            std::string key = scm_to_locale_string(scm_symbol_to_string(this->_scheme));
            sbn::upstream<sbn::Local>(
                this, 
                sbn::make_pointer<sbn::guile::expression_kernel_define>(
                    this->_definitions[key], 
                    this->_environment,
                    this->_definitions,
                    SCM_UNSPECIFIED,
                    -1
                )
            );
            this->_args.resize(1);
        } else {
            bool is_symbol = scm_is_true(scm_symbol_p(this->_scheme));
            if (is_symbol) {
                SCM defined = scm_module_variable(this->_environment, this->_scheme);
                if (scm_is_false(defined) || scm_is_true(scm_variable_p(defined))) {
                    this->_result = scm_variable_ref(defined);
                } else {
                    this->_result = this->_scheme;
                }
            }
            // this->_result = this->_scheme;
            sbn::commit<sbn::Local>(std::move(this_ptr()));
        }
    }
}

void
sbn::guile::expression_kernel::react(sbn::kernel_ptr&& child) {
    auto child_kernel = sbn::pointer_dynamic_cast<sbn::guile::expression_kernel>(std::move(child));
    for (const auto& pair: child_kernel->get_defs()) {
        this->_definitions[pair.first] = pair.second;
    }
    int arg = child_kernel->get_arg();
    if (arg < 0) {
        //if or define kernel
        this->_result = child_kernel->get_result();
        sbn::commit<sbn::Local>(std::move(this_ptr()));
        return;
    } 
    _args[arg] = child_kernel->get_result();;
    _finished_child++;
    if (_finished_child == _args.size()) {
        _result = scm_list_1(_args.front());
        for (size_t i = 1; i < _args.size(); i++) {
            bool is_list = bool(scm_is_true(scm_list_p(this->_args[i])));
            //dumb check for lists like (1 2 3)
            bool check = is_list && bool(scm_is_false(scm_symbol_p(this->_args[i])));
            if (check) {
                this->_args[i] = scm_list_2(scm_sym_quote, this->_args[i]);
            }
            _result = scm_append(scm_list_2(_result, scm_list_1(_args[i])));
        }
        this->_result = scm_eval(this->_result, this->_environment);
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    }
}

void
sbn::guile::expression_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    //in >> this->_parent_arg;
    //in >> this->_scheme;
}

void
sbn::guile::expression_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    //out >> this->_parent_arg;
    //out >> this->_scheme;
}

void 
sbn::guile::expression_kernel_if::act() {
    protected_scm if_statement = scm_car(scm_cdr(this->_scheme));
    sbn::upstream<sbn::Local>(
        this, 
        sbn::make_pointer<sbn::guile::expression_kernel>(
            if_statement, 
            this->_environment, 
            this->_definitions, 
            0
        )
    );
}

void 
sbn::guile::expression_kernel_if::react(sbn::kernel_ptr&& child) {
    auto child_kernel = sbn::pointer_dynamic_cast<sbn::guile::expression_kernel>(std::move(child));
    for (const auto& pair: child_kernel->get_defs()) {
        this->_definitions[pair.first] = pair.second;
    }
    int arg = child_kernel->get_arg();
    
    // 0 -- sign of statement

    if (arg == 0) {
        SCM statement = child_kernel->get_result();

        bool is_true = bool(scm_is_true(statement));
        if (is_true) {
            SCM result = scm_car(scm_cdr(scm_cdr(this->_scheme)));            
            sbn::upstream<sbn::Local>(
                this, 
                sbn::make_pointer<sbn::guile::expression_kernel>(
                    result, 
                    this->_environment, 
                    this->_definitions, 
                    -1
                )
            );
        } else {
            SCM result = scm_car(scm_cdr(scm_cdr(scm_cdr(this->_scheme))));

            sbn::upstream<sbn::Local>(
                this, 
                sbn::make_pointer<sbn::guile::expression_kernel>(
                    result, 
                    this->_environment, 
                    this->_definitions, 
                    -1
                )
            );
        }
    }
    else if (arg < 0) {
        _result = child_kernel->get_result();
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    }
}

void
sbn::guile::expression_kernel_if::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    //in >> this->_parent_arg;
    //in >> this->_scheme;
}

void
sbn::guile::expression_kernel_if::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    //out >> this->_parent_arg;
    //out >> this->_scheme;
}

sbn::guile::expression_kernel_define::
expression_kernel_define(SCM scm, std::map<std::string, SCM> const & def, protected_scm args, int arg)
: expression_kernel(scm, def, arg) {
    //(define (head) (body))
    this->_head = scm_car(scm_cdr(scm));
    this->_body = scm_car(scm_cdr(scm_cdr(scm)));
    this->_body_args = args;
}

sbn::guile::expression_kernel_define::
expression_kernel_define(SCM scm, SCM env, std::map<std::string, SCM> const & def, protected_scm args, int arg)
: expression_kernel(scm, env, def, arg) {
    //(define (head) (body))
    this->_head = scm_car(scm_cdr(scm));
    this->_body = scm_car(scm_cdr(scm_cdr(scm)));
    this->_body_args = args;
}



void 
sbn::guile::expression_kernel_define::act() {
    int arg_number = 0;
    if (!scm_is_true(scm_list_p(this->_body_args))) {
        sbn::upstream<sbn::Local>(
            this, 
            sbn::make_pointer<sbn::guile::expression_kernel>(
                this->_body, 
                this->_environment,
                this->_definitions, 
                -1
            )
        );
        this->_args.resize(1);
    } else {
        for (SCM s = _body_args; s != SCM_EOL; s = scm_cdr(s)) {
            sbn::upstream<sbn::Local>(
                this, 
                sbn::make_pointer<sbn::guile::expression_kernel>(
                    scm_car(s), 
                    this->_environment,
                    this->_definitions, 
                    arg_number++
                )
            );
        }
        this->_args.resize(arg_number);
    }
}

void 
sbn::guile::expression_kernel_define::react(sbn::kernel_ptr&& child) {
    auto child_kernel = sbn::pointer_dynamic_cast<sbn::guile::expression_kernel>(std::move(child));
    int arg = child_kernel->get_arg();
    SCM result = child_kernel->get_result();
    for (const auto& pair: child_kernel->get_defs()) {
        this->_definitions[pair.first] = pair.second;
    }
    if (arg < 0) {
        this->_result = result;
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    } else if (this->_args.size() ==0) {
        this->_result = result;
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    } else {
    //functions like (define (f x y) (+ x y))
        this->_args[arg] = result;
        this->_finished_child++;
        if (_finished_child == _args.size()) {
            SCM args = scm_cdr(this->_head);
            int i = 0;
            SCM env = this->_environment;
            for (SCM s = args; s != SCM_EOL; s = scm_cdr(s)) {se(scm_eqv_p(scm_public_variable(env, scm_symbol_to_string(scm_car(s))), scm_car(s)))) {
                    scm_module_define(env, scm_car(s), this->_args[i++]);
            }
            
            auto child = sbn::make_pointer<sbn::guile::expression_kernel>(
                this->_body, 
                env,
                this->_definitions,
                -3
            );
            sbn::upstream<sbn::Local>(
                this, 
                std::move(child)
            );
            
        }
    } 
}

void
sbn::guile::expression_kernel_define::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    //in >> this->_parent_arg;
    //in >> this->_scheme;
}

void
sbn::guile::expression_kernel_define::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    //out >> this->_parent_arg;
    //out >> this->_scheme;
}