#ifndef COVER_UPDATE_IMPL_HPP
#define COVER_UPDATE_IMPL_HPP

#include "CoverMovement.hpp"
#include "CoverMovementContext.hpp"
#include "CoverStop.hpp"
#include "CoverUpdate.hpp"

class CoverUpdateImpl : public CoverUpdate {
public:
    CoverUpdateImpl(
        CoverMovementContext& context, CoverMovement& up, CoverMovement& down,
        CoverStop& stopper);

    void update(Actions& action) override;

private:
    void log(const std::string& msg);

    CoverMovementContext& context;
    CoverMovement& up;
    CoverMovement& down;
    CoverStop& stopper;
};

#endif  // COVER_UPDATE_IMPL_HPP
