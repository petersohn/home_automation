#ifndef COVER_UPDATE_HPP
#define COVER_UPDATE_HPP

#include "Actions.hpp"
#include "CoverMovement.hpp"
#include "CoverState.hpp"
#include "CoverStop.hpp"

class CoverUpdate {
public:
    CoverUpdate(
        CoverState& context, CoverMovement& up, CoverMovement& down,
        CoverStop& stopper);

    void update(Actions& action);
    void requestOpen();

private:
    void log(const std::string& msg);

    CoverState& context;
    CoverMovement& up;
    CoverMovement& down;
    CoverStop& stopper;
};

#endif  // COVER_UPDATE_HPP
