#ifndef TEST_INTERFACETESTBASE_HPP
#define TEST_INTERFACETESTBASE_HPP

#include "EspTestBase.hpp"
#include "common/Actions.hpp"
#include "common/InterfaceConfig.hpp"

class InterfaceTestBase : public EspTestBase {
public:
    InterfaceConfig interface;

    void initInterface(std::string name, std::unique_ptr<Interface>&& iface);
    void updateInterface();
    std::string getValue(size_t index);

private:
    Actions actions{interface};
};

#endif  // TEST_INTERFACETESTBASE_HPP
