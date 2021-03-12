#ifndef SUBORDINATION_CORE_FACTORY_PROPERTIES_HH
#define SUBORDINATION_CORE_FACTORY_PROPERTIES_HH

#include <unistdx/ipc/process>

#include <subordination/core/child_process_pipeline.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/core/properties.hh>

namespace sbn {

    class Properties: public properties {

    public:
        parallel_pipeline::properties local;
        child_process_pipeline::properties remote;

    public:
        Properties();
        void property(const std::string& key, const std::string& value) override;

    };

}

#endif // vim:filetype=cpp
