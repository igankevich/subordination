#include <fstream>
#include <iomanip>

#include <unistdx/util/system>

#include <subordination/core/basic_factory.hh>
#include <subordination/core/properties.hh>

namespace  {
    inline unsigned num_threads() {
        unsigned n = 1;
        const char* sbn_num_threads = std::getenv("SBN_NUM_THREADS");
        if (sbn_num_threads) {
            n = std::atoi(sbn_num_threads);
            if (n < 1) { n = 1; }
        } else {
            n = sys::thread_concurrency();
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
        size_t _min_output_buffer_size = std::numeric_limits<size_t>::max();
        size_t _min_input_buffer_size = std::numeric_limits<size_t>::max();

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
                return;
            }
            if (key == "local.num-downstream-threads") {
                auto n = std::stoi(value);
                if (n < 0) { throw std::out_of_range("out of range"); }
                this->_num_downstream_threads = n;
                return;
            }
            if (key == "remote.min-input-buffer-size") {
                auto n = std::stoul(value);
                this->_min_input_buffer_size = n;
                return;
            }
            if (key == "remote.min-output-buffer-size") {
                auto n = std::stoul(value);
                this->_min_output_buffer_size = n;
                return;
            }
            throw std::runtime_error("unknown property");
        }

        inline void init_default_values() {
            if (!is_set(this->_num_upstream_threads)) {
                this->_num_upstream_threads = num_threads();
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
    this->_local.name("app local");
    this->_local.error_pipeline(&this->_remote);
    this->_remote.min_input_buffer_size(config._min_input_buffer_size);
    this->_remote.min_output_buffer_size(config._min_output_buffer_size);
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
