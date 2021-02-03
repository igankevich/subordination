#include <fstream>
#include <iomanip>

#include <subordination/core/basic_factory.hh>

sbn::Factory::Factory(const Properties& config): _local(config.local) {
    this->_local.name("app local");
    this->_local.error_pipeline(&this->_remote);
    this->_remote.cpus(config.remote.cpus);
    this->_remote.min_input_buffer_size(config.remote.min_input_buffer_size);
    this->_remote.min_output_buffer_size(config.remote.min_output_buffer_size);
    this->_remote.pipe_buffer_size(config.remote.pipe_buffer_size);
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
