#include <subordination/core/pipeline_base.hh>

void sbn::pipeline::forward(kernel_ptr&& k) {
    this->log("forward is not implemented, deleting _", *k);
}

void sbn::pipeline::recover(kernel_ptr&& k) {
    this->log("recover is not implemented, deleting _", *k);
}

const char* sbn::to_string(pipeline_base::states rhs) {
    using s = pipeline_base::states;
    switch (rhs) {
        case s::initial: return "initial";
        case s::starting: return "starting";
        case s::started: return "started";
        case s::stopping: return "stopping";
        case s::stopped: return "stopped";
        default: return nullptr;
    }
}

std::ostream& sbn::operator<<(std::ostream& out, pipeline_base::states rhs) {
    if (auto* s = to_string(rhs)) {
        out << s;
    } else {
        out << "unknown(";
        out << static_cast<std::underlying_type<pipeline_base::states>::type>(rhs);
        out << ')';
    }
    return out;
}
