#include <subordination/core/pipeline_base.hh>

void sbn::pipeline::forward(foreign_kernel_ptr&& k) {
    this->log("forward is not implemented, deleting _", *k);
}

void sbn::pipeline::recover(kernel_ptr&& k) {
    this->log("recover is not implemented, deleting _", *k);
}

const char* sbn::to_string(pipeline_state rhs) {
    switch (rhs) {
        case pipeline_state::initial: return "initial";
        case pipeline_state::starting: return "starting";
        case pipeline_state::started: return "started";
        case pipeline_state::stopping: return "stopping";
        case pipeline_state::stopped: return "stopped";
        default: return nullptr;
    }
}

std::ostream& sbn::operator<<(std::ostream& out, pipeline_state rhs) {
    if (auto* s = to_string(rhs)) {
        out << s;
    } else {
        out << "unknown(";
        out << static_cast<std::underlying_type<pipeline_state>::type>(rhs);
        out << ')';
    }
    return out;
}
