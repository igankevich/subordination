#include <fstream>
#include <iomanip>
#include <sstream>

#include <unistdx/system/resource>

#include <subordination/core/basic_factory.hh>
#include <subordination/core/properties.hh>

namespace  {
    inline unsigned num_threads(const sys::cpu_set& available_cpus) {
        unsigned n = 1;
        const char* sbn_num_threads = std::getenv("SBN_NUM_THREADS");
        if (sbn_num_threads) {
            n = std::atoi(sbn_num_threads);
            if (n < 1) { n = 1; }
        } else {
            n = available_cpus.count();
        }
        return n;
    }
    inline unsigned num_downstream_threads() {
        unsigned n = 0;
        const char* sbn_num_threads = std::getenv("SBN_NUM_DOWNSTREAM_THREADS");
        if (sbn_num_threads) { n = std::atoi(sbn_num_threads); }
        return n;
    }

    template <class T> inline bool is_set(T value) {
        return value != std::numeric_limits<T>::max();
    }

    class Properties: public sbn::properties {

    public:
        unsigned _num_upstream_threads = std::numeric_limits<unsigned>::max();
        unsigned _num_downstream_threads = 0;
        sys::cpu_set _upstream_cpus;
        sys::cpu_set _downstream_cpus;
        sys::cpu_set _timer_cpus;
        size_t _min_output_buffer_size = std::numeric_limits<size_t>::max();
        size_t _min_input_buffer_size = std::numeric_limits<size_t>::max();
        size_t _pipe_buffer_size = std::numeric_limits<size_t>::max();

    public:
        inline Properties() {
            if (const char* filename = std::getenv("SBN_CONFIG")) { open(filename); }
            init_default_values();
        }

        void property(const std::string& key, const std::string& value) override {
            if (key == "local.num-upstream-threads") {
                auto n = std::stoi(value);
                if (n < 1) { throw std::out_of_range("out of range"); }
                this->_num_upstream_threads = n;
            } else if (key == "local.num-downstream-threads") {
                auto n = std::stoi(value);
                if (n < 0) { throw std::out_of_range("out of range"); }
                this->_num_downstream_threads = n;
            } else if (key == "local.upstream-cpus") {
                std::stringstream tmp(value);
                tmp >> this->_upstream_cpus;
                if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
            } else if (key == "local.downstream-cpus") {
                std::stringstream tmp(value);
                tmp >> this->_downstream_cpus;
                if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
            } else if (key == "local.timer-cpus") {
                std::stringstream tmp(value);
                tmp >> this->_timer_cpus;
                if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
            } else if (key == "remote.min-input-buffer-size") {
                auto n = std::stoul(value);
                this->_min_input_buffer_size = n;
            } else if (key == "remote.min-output-buffer-size") {
                auto n = std::stoul(value);
                this->_min_output_buffer_size = n;
            } else if (key == "remote.pipe-buffer-size") {
                auto n = std::stoul(value);
                this->_pipe_buffer_size = n;
            } else {
                throw std::invalid_argument("unknown property");
            }
        }

        inline void init_default_values() {
            const auto& available_cpus = sys::this_process::cpu_affinity();
            this->_upstream_cpus &= available_cpus;
            if (this->_upstream_cpus.count() == 0) {
                this->_upstream_cpus = available_cpus;
            }
            this->_downstream_cpus &= available_cpus;
            this->_timer_cpus &= available_cpus;
            if (!is_set(this->_num_upstream_threads)) {
                this->_num_upstream_threads = num_threads(this->_upstream_cpus);
            }
            if (this->_num_downstream_threads == 0) {
                this->_num_downstream_threads = num_downstream_threads();
            }
            if (!is_set(this->_min_input_buffer_size)) {
                this->_min_input_buffer_size = sys::page_size()*16;
            }
            if (!is_set(this->_min_output_buffer_size)) {
                this->_min_output_buffer_size = sys::page_size()*16;
            }
        }

    };

}

sbn::Factory::Factory() {
    Properties config;
    this->_local.num_upstream_threads(config._num_upstream_threads);
    this->_local.num_downstream_threads(config._num_downstream_threads);
    this->_local.upstream_threads_cpus(config._upstream_cpus);
    this->_local.downstream_threads_cpus(config._downstream_cpus);
    this->_local.timer_threads_cpus(config._timer_cpus);
    this->_local.name("app local");
    this->_local.error_pipeline(&this->_remote);
    this->_remote.min_input_buffer_size(config._min_input_buffer_size);
    this->_remote.min_output_buffer_size(config._min_output_buffer_size);
    this->_remote.pipe_buffer_size(config._pipe_buffer_size);
    this->_remote.name("app remote");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_remote);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.types(&this->_types);
    this->_remote.instances(&this->_instances);
    this->_remote.add_connection();
}

void sbn::Factory::start() {
    this->_local.start();
    this->_remote.start();
}

void sbn::Factory::stop() {
    this->_local.stop();
    this->_remote.stop();
}

void sbn::Factory::wait() {
    this->_local.wait();
    this->_remote.wait();
}

void sbn::Factory::clear() {
    kernel_sack sack;
    this->_local.clear(sack);
    this->_remote.clear(sack);
}

sbn::Factory sbn::factory;
