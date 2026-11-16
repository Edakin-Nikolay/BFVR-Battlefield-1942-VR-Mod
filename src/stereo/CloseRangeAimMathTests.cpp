#include "stereo/CloseRangeAimMath.h"

#include <cmath>
#include <cstdio>

namespace
{
bool Expect(bool ok, const char* message) noexcept
{
    if (!ok)
    {
        std::fprintf(stderr, "CloseRangeAimMath: %s\n", message);
    }
    return ok;
}

bool Near(float a, float b) noexcept
{
    return std::fabs(a - b) <= 0.0001F;
}

bfvr::stereo::Matrix4 IdentityGun() noexcept
{
    bfvr::stereo::Matrix4 m = {};
    m.values[0][0] = 1.0F;
    m.values[1][1] = 1.0F;
    m.values[2][2] = 1.0F;
    m.values[3][3] = 1.0F;
    return m;
}

bool ZeroBarrelIsZeroParallax() noexcept
{
    const auto gun = IdentityGun();
    for (const float d : {2.0F, 50.0F})
    {
        const auto r = bfvr::stereo::MeasureCloseRangeAimParallax(
            gun, {0, 0, 0}, {0, 0, 1}, d);
        if (!Expect(r.has_value() && Near(r->lateralMeters, 0) &&
                Near(r->verticalMeters, 0) && Near(r->basisAngularDegrees, 0),
                "aligned barrel produced parallax"))
        {
            return false;
        }
    }
    return true;
}

bool LateralBarrelOffsetIsConstantOriginParallax() noexcept
{
    const auto gun = IdentityGun();
    const auto near = bfvr::stereo::MeasureCloseRangeAimParallax(
        gun, {0.1F, 0, 0}, {0, 0, 1}, 2.0F);
    const auto far = bfvr::stereo::MeasureCloseRangeAimParallax(
        gun, {0.1F, 0, 0}, {0, 0, 1}, 50.0F);
    return Expect(near.has_value() && far.has_value() &&
            Near(near->lateralMeters, 0.1F) && Near(far->lateralMeters, 0.1F) &&
            Near(near->verticalMeters, 0) && Near(near->basisAngularDegrees, 0),
        "lateral barrel offset did not give constant origin parallax");
}

bool AngularBasisErrorGrowsWithDistance() noexcept
{
    const auto gun = IdentityGun();
    const float a = 1.0F * 3.14159265358979F / 180.0F;
    const bfvr::stereo::Vec3 dir = {std::sin(a), 0, std::cos(a)};
    const auto near = bfvr::stereo::MeasureCloseRangeAimParallax(gun, {0, 0, 0}, dir, 2.0F);
    const auto far = bfvr::stereo::MeasureCloseRangeAimParallax(gun, {0, 0, 0}, dir, 50.0F);
    return Expect(near.has_value() && far.has_value() &&
            Near(near->basisAngularDegrees, 1.0F) &&
            far->lateralMeters > near->lateralMeters &&
            Near(near->lateralMeters, 2.0F * std::tan(a)) &&
            Near(far->lateralMeters, 50.0F * std::tan(a)),
        "angular basis error did not scale with distance");
}

bool FailClosed() noexcept
{
    const auto gun = IdentityGun();
    const float nan = std::nanf("");
    return Expect(!bfvr::stereo::MeasureCloseRangeAimParallax(gun, {0, 0, 0}, {0, 0, 1}, -1).has_value(), "negative distance accepted") &&
        Expect(!bfvr::stereo::MeasureCloseRangeAimParallax(gun, {0, 0, 0}, {0, 0, 0}, 2).has_value(), "zero barrel dir accepted") &&
        Expect(!bfvr::stereo::MeasureCloseRangeAimParallax(gun, {0, 0, 0}, {0, 0, -1}, 2).has_value(), "backward barrel dir accepted") &&
        Expect(!bfvr::stereo::MeasureCloseRangeAimParallax(gun, {nan, 0, 0}, {0, 0, 1}, 2).has_value(), "non-finite offset accepted");
}
} // namespace

int main()
{
    if (!ZeroBarrelIsZeroParallax() ||
        !LateralBarrelOffsetIsConstantOriginParallax() ||
        !AngularBasisErrorGrowsWithDistance() ||
        !FailClosed())
    {
        return 1;
    }
    std::printf("Close-range aim math tests passed.\n");
    return 0;
}
