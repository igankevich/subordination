#include <subordination/guile/expression_kernel.hh>

#include <iostream>

sbn::guile::expression_kernel_main::
expression_kernel_main(int argc, char* argv[]) {
    if (argc > 1) {
        SCM port = scm_open_file(
            scm_from_locale_string (argv[1]), 
            scm_from_locale_string ("r")
        );
        _scheme = scm_read(port);
    }

}

void 
sbn::guile::expression_kernel_main::act() {
    if (_scheme) {
        sbn::upstream<sbn::Remote>(this, sbn::make_pointer<sbn::guile::expression_kernel>(_scheme));
    }
    sbn::commit<sbn::Remote>(std::move(this_ptr()));
}

void 
sbn::guile::expression_kernel_main::react(sbn::kernel_ptr&& child) {
    sbn::commit<sbn::Remote>(std::move(this_ptr()));
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
        for (SCM s = this->_scheme; s != SCM_EOL; s = scm_cdr(s)) {
            _args.emplace_back(scm_car(s));
        }
        for (size_t i = 0; i < _args.size(); i++) {
            sbn::upstream<sbn::Remote>(this, sbn::make_pointer<sbn::guile::expression_kernel>(_args[i], i));
        }
    } else {
        _args.emplace_back(this->_scheme);
        // this->_result = scm_eval(this->_scheme, scm_interaction_environment());
        this->_result = _scheme;
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    }
}

void
sbn::guile::expression_kernel::react(sbn::kernel_ptr&& child) {
    auto child_kernel = sbn::pointer_dynamic_cast<sbn::guile::expression_kernel>(std::move(child));
    SCM result = child_kernel->get_result();
    int arg = child_kernel->get_arg();
    _args[arg] = result;
    _finished_child++;
    if (_finished_child == _args.size()) {
       // _result = scm_list_2(_args.front(), scm_list_1(_args.back()));
        _result = scm_list_1(_args.front());
        for (size_t i = 1; i < _args.size(); i++) {
            _result = scm_append(scm_list_2(_result, scm_list_1(_args[i])));
        }
        scm_display(_result, scm_current_output_port());
        this->_result = scm_eval(this->_result, scm_interaction_environment());
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