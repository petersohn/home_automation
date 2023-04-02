#include "InterfaceTestBase.hpp"
#include "common/Interface.hpp"

#include "boost/test/unit_test.hpp"

void InterfaceTestBase::initInterface(
        std::string name, std::unique_ptr<Interface>&& iface) {
    interface.name = std::move(name);
    interface.interface = std::move(iface);
    interface.interface->start();
}

void InterfaceTestBase::updateInterface() {
    interface.interface->update(actions);
}

std::string InterfaceTestBase::getValue(size_t index) {
    BOOST_REQUIRE(index < interface.storedValue.size());
    return interface.storedValue[index];
}
