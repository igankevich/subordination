#ifndef SUBORDINATION_CORE_FACTORY_PROPERTIES_HH
#define SUBORDINATION_CORE_FACTORY_PROPERTIES_HH

#include <unistdx/ipc/process>

#include <subordination/core/parallel_pipeline.hh>
#include <subordination/core/properties.hh>

namespace sbn {

    class Properties: public properties {

    public:
        sbn::parallel_pipeline::properties local;
        struct Remote {
            sys::cpu_set cpus;
            size_t min_output_buffer_size = std::numeric_limits<size_t>::max();
            size_t min_input_buffer_size = std::numeric_limits<size_t>::max();
            size_t pipe_buffer_size = std::numeric_limits<size_t>::max();
        } remote;

    public:
        Properties();
        void property(const std::string& key, const std::string& value) override;

    };

}

#endif // vim:filetype=cpp
