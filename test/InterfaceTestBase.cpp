#include "InterfaceTestBase.hpp"

#include "boost/test/unit_test.hpp"
#include "common/Interface.hpp"

void InterfaceTestBase::initInterface(
    std::string name, std::unique_ptr<Interface>&& iface) {
    this->interface.name = std::move(name);
    this->interface.interface = std::move(iface);
    this->interface.interface->start();
}

void InterfaceTestBase::updateInterface() {
    this->interface.interface->update(this->actions);
}

std::string InterfaceTestBase::getValue(size_t index) {
    BOOST_REQUIRE(index < this->interface.storedValue.size());
    return this->interface.storedValue[index];
}
