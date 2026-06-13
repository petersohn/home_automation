#ifndef COVER_UPDATE_HPP
#define COVER_UPDATE_HPP

#include "Actions.hpp"

class CoverUpdate {
public:
    virtual ~CoverUpdate() = default;
    virtual void update(Actions& action) = 0;
};

#endif  // COVER_UPDATE_HPP
