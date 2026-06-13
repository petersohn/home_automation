#ifndef COVER_UPDATE_HPP
#define COVER_UPDATE_HPP

#include "Actions.hpp"
#include "CoverMovement.hpp"
#include "CoverMovementContext.hpp"
#include "CoverStop.hpp"

class CoverUpdate {
public:
    CoverUpdate(
        CoverMovementContext& context, CoverMovement& up, CoverMovement& down,
        CoverStop& stopper);

    void update(Actions& action);

private:
    void log(const std::string& msg);

    CoverMovementContext& context;
    CoverMovement& up;
    CoverMovement& down;
    CoverStop& stopper;
};

#endif  // COVER_UPDATE_HPP
