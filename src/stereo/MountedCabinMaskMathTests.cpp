#include "stereo/MountedCabinMaskMath.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace
{

constexpr float kPi = 3.14159265358979323846F;

bool Expect(
    const bool condition,
    const std::string_view test,
    const char* message) noexcept
{
    if (!condition)
    {
        std::printf(
            "FAIL [%.*s]: %s\n",
            static_cast<int>(test.size()),
            test.data(),
            message);
        return false;
    }
    return true;
}

float Radians(const float degrees) noexcept
{
    return degrees * kPi / 180.0F;
}

bfvr::stereo::MountedCabinWindow Window() noexcept
{
    bfvr::stereo::MountedCabinWindow window = {};
    window.forwardWorld = {0.0F, 0.0F, 1.0F};
    window.rightWorld = {1.0F, 0.0F, 0.0F};
    window.upWorld = {0.0F, 1.0F, 0.0F};
    window.halfHorizontalRadians = Radians(30.0F);
    window.halfVerticalRadians = Radians(15.0F);
    window.featherRadians = Radians(5.0F);
    return window;
}

bfvr::stereo::Vec3 Direction(
    const float horizontalDegrees,
    const float verticalDegrees) noexcept
{
    const float horizontal = Radians(horizontalDegrees);
    const float vertical = Radians(verticalDegrees);
    const float x = std::tan(horizontal);
    const float y = std::tan(vertical);
    const float length = std::sqrt(x * x + y * y + 1.0F);
    return {x / length, y / length, 1.0F / length};
}

bool CenterAndInsideAreClear() noexcept
{
    constexpr std::string_view test = "center/inside";
    const auto window = Window();
    const auto center =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(0.0F, 0.0F),
            window);
    const auto inside =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(20.0F, 10.0F),
            window);
    return Expect(
               center.has_value() && center->inside &&
                   std::fabs(center->alpha) < 0.0001F,
               test,
               "center direction was not clear") &&
        Expect(
            inside.has_value() && inside->inside &&
                std::fabs(inside->alpha) < 0.0001F,
            test,
            "inside direction was not clear");
}

bool FeatherAndOutsideAreMasked() noexcept
{
    constexpr std::string_view test = "feather/outside";
    const auto window = Window();
    const auto feathered =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(32.0F, 0.0F),
            window);
    const auto outside =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(40.0F, 0.0F),
            window);
    return Expect(
               feathered.has_value() && !feathered->inside &&
                   feathered->alpha > 0.0F &&
                   feathered->alpha < 1.0F,
               test,
               "feather band did not produce a partial mask") &&
        Expect(
            outside.has_value() && !outside->inside &&
                std::fabs(outside->alpha - 1.0F) < 0.0001F,
            test,
            "outside direction was not fully masked");
}

bool BehindIsMaskedAndInvalidWindowFailsClosed() noexcept
{
    constexpr std::string_view test = "behind/invalid";
    const auto window = Window();
    const auto behind =
        bfvr::stereo::EvaluateMountedCabinMask(
            {0.0F, 0.0F, -1.0F},
            window);
    auto invalid = window;
    invalid.halfHorizontalRadians = 0.0F;
    const auto rejected =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(0.0F, 0.0F),
            invalid);
    auto nonOrthogonal = window;
    nonOrthogonal.rightWorld = {0.0F, 0.0F, 1.0F};
    const auto rejectedBasis =
        bfvr::stereo::EvaluateMountedCabinMask(
            Direction(0.0F, 0.0F),
            nonOrthogonal);
    return Expect(
               behind.has_value() && !behind->inside &&
                   std::fabs(behind->alpha - 1.0F) < 0.0001F,
               test,
               "behind direction was not masked") &&
        Expect(
            !rejected.has_value() && !rejectedBasis.has_value(),
            test,
            "invalid window did not fail closed");
}

bool ClampKeepsInsideAndBoundsOutside() noexcept
{
    constexpr std::string_view test = "clamp";
    const auto window = Window();
    const auto inside = Direction(10.0F, 5.0F);
    const auto kept =
        bfvr::stereo::ClampForwardToMountedCabinWindow(inside, window);
    const auto outside = Direction(45.0F, 20.0F);
    const auto clamped =
        bfvr::stereo::ClampForwardToMountedCabinWindow(outside, window);
    const auto clampedSample = clamped.has_value()
        ? bfvr::stereo::EvaluateMountedCabinMask(*clamped, window)
        : std::nullopt;
    const auto behind =
        bfvr::stereo::ClampForwardToMountedCabinWindow(
            {0.0F, 0.0F, -1.0F},
            window);
    const auto behindSample = behind.has_value()
        ? bfvr::stereo::EvaluateMountedCabinMask(*behind, window)
        : std::nullopt;
    return Expect(
               kept.has_value() &&
                   std::fabs(kept->x - inside.x) < 0.0001F &&
                   std::fabs(kept->y - inside.y) < 0.0001F &&
                   std::fabs(kept->z - inside.z) < 0.0001F,
               test,
               "inside direction changed during clamp") &&
        Expect(
            clamped.has_value() && clampedSample.has_value() &&
                clampedSample->inside,
            test,
            "outside direction was not clamped into the window") &&
        Expect(
            behind.has_value() && behindSample.has_value() &&
                behindSample->inside,
            test,
            "behind direction was not clamped into the window");
}

} // namespace

int wmain()
{
    const bool passed =
        CenterAndInsideAreClear() &&
        FeatherAndOutsideAreMasked() &&
        BehindIsMaskedAndInvalidWindowFailsClosed() &&
        ClampKeepsInsideAndBoundsOutside();
    if (!passed)
    {
        std::printf("Mounted cabin mask math tests failed.\n");
        return 1;
    }
    std::printf("Mounted cabin mask math tests passed.\n");
    return 0;
}
