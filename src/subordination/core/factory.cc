#include <fstream>
#include <iomanip>

#include <subordination/core/factory.hh>
#include <subordination/core/list.hh>

sbn::Factory::Factory(const Properties& config): _local(config.local), _remote(config.remote) {
    this->_local.name("app local");
    this->_local.error_pipeline(&this->_remote);
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

void sbn::Factory::write(std::ostream& out) const {
    using sbn::list;
    out << '(';
    out << list("local", this->_local);
    out << ' ';
    out << list("remote", this->_remote);
    out << ' ';
    out << list("types", this->_types);
    out << ' ';
    out << list("instances", this->_instances);
    out << ')';
}
