#ifndef OPERATION_OPERATION_HPP
#define OPERATION_OPERATION_HPP

#include <string>

namespace operation {

class Operation {
public:
    virtual std::string evaluate() = 0;
    virtual ~Operation() {}
};

}  // namespace operation

#endif  // OPERATION_OPERATION_HPP
