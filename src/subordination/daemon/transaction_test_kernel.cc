#include <unistdx/base/log_message>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/main_kernel.hh>
#include <subordination/daemon/transaction_test_kernel.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("tr", args...);
}

void sbnd::Transaction_test_kernel::act() {
    auto k = sbn::make_pointer<Foreign_main_kernel>();
    auto* a = new sbn::application(this->_application);
    if (auto* src = source_application()) {
        a->credentials(src->user(), src->group());
    }
    k->target_application(a);
    k->parent(this);
    sbnd::factory.process().send(std::move(k));
}

void sbnd::Transaction_test_kernel::react(sbn::kernel_ptr&& k) {
    const auto& t = typeid(*k);
    if (t == typeid(Foreign_main_kernel)) {
        if (this->_exit_codes.empty()) {
            log("recv main kernel _", *k);
            this->_exit_codes.emplace_back(k->return_code());
            auto s = sbn::make_pointer<Transaction_gather_superior>();
            s->parent(this);
            s->application_id(k->source_application_id());
            sbnd::factory.local().send(std::move(s));
        } else {
            const auto nreturned = this->_exit_codes.size();
            log("recv main kernel _/_ _", nreturned, nreturned + this->_records.size(),
                k->return_code());
            if (this->_records.empty()) {
                return_to_parent();
                sbnd::factory.unix().send(std::move(this_ptr()));
            } else {
                this->_records.pop_back();
                sbnd::factory.transactions().recover(this->_records);
            }
        }
    } else if (t == typeid(Transaction_gather_superior)) {
        log("recv transaction gather superior kernel _", *k);
        auto s = sbn::pointer_dynamic_cast<Transaction_gather_superior>(std::move(k));
        this->_records = std::move(s->records());
        if (this->_records.empty()) {
            log("no records to recover");
            return_to_parent();
            sbnd::factory.unix().send(std::move(this_ptr()));
        } else {
            log("recover _ records", this->_records.size());
            sbnd::factory.transactions().recover(this->_records);
        }
    }
}

void sbnd::Transaction_test_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_application << this->_exit_codes;
}

void sbnd::Transaction_test_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_application >> this->_exit_codes;
}

void sbnd::Transaction_gather_superior::act() {
    auto g = sbnd::factory.remote().guard();
    const auto& clients = sbnd::factory.remote().clients();
    this->_nkernels = clients.size();
    for (const auto& pair : clients) {
        auto k = sbn::make_pointer<Transaction_gather_subordinate>();
        k->destination(pair.first);
        k->application_id(application_id());
        k->parent(this);
        sbnd::factory.remote().send(std::move(k));
    }
}

void sbnd::Transaction_gather_superior::react(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(Transaction_gather_subordinate)) {
        --this->_nkernels;
        auto s = sbn::pointer_dynamic_cast<Transaction_gather_subordinate>(std::move(k));
        for (auto& r : s->records()) {
            // deduplicate
            auto result = std::find_if(this->_records.begin(), this->_records.end(),
                                       [&r] (const sbn::transaction_record& b) {
                                           return *r.k == *b.k;
                                       });
            if (result == this->_records.end()) {
                this->_records.emplace_back(std::move(r));
            }
        }
    }
    if (this->_nkernels == 0) {
        return_to_parent();
        sbnd::factory.local().send(std::move(this_ptr()));
    }
}

void sbnd::Transaction_gather_subordinate::act() {
    this->_records = sbnd::factory.transactions().select(this->_application_id);
    return_to_parent();
    sbnd::factory.remote().send(std::move(this_ptr()));
}

void sbnd::Transaction_gather_subordinate::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_records << this->_application_id;
}

void sbnd::Transaction_gather_subordinate::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_records >> this->_application_id;
}
