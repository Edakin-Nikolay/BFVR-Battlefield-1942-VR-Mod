#include "stereo/MountedCabinMaskMath.h"

#include <algorithm>
#include <cmath>

namespace bfvr::stereo
{

namespace
{

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinimumAxisLength = 0.5F;
constexpr float kMaximumOrthogonalityError = 0.01F;
constexpr float kMinimumForwardComponent = 0.0001F;

bool IsFinite(const Vec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

float Length(const Vec3& value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

float Dot(const Vec3& left, const Vec3& right) noexcept
{
    return left.x * right.x +
        left.y * right.y +
        left.z * right.z;
}

std::optional<Vec3> Normalize(const Vec3& value) noexcept
{
    const float length = Length(value);
    if (!std::isfinite(length) || length < kMinimumAxisLength)
    {
        return std::nullopt;
    }
    return Vec3{
        value.x / length,
        value.y / length,
        value.z / length};
}

bool IsValidWindow(const MountedCabinWindow& window) noexcept
{
    if (!IsFinite(window.forwardWorld) ||
        !IsFinite(window.rightWorld) ||
        !IsFinite(window.upWorld) ||
        !std::isfinite(window.halfHorizontalRadians) ||
        !std::isfinite(window.halfVerticalRadians) ||
        !std::isfinite(window.featherRadians))
    {
        return false;
    }
    if (window.halfHorizontalRadians <= 0.0F ||
        window.halfVerticalRadians <= 0.0F ||
        window.halfHorizontalRadians >= kPi * 0.5F ||
        window.halfVerticalRadians >= kPi * 0.5F ||
        window.featherRadians < 0.0F ||
        window.featherRadians > kPi * 0.5F)
    {
        return false;
    }
    const auto forward = Normalize(window.forwardWorld);
    const auto right = Normalize(window.rightWorld);
    const auto up = Normalize(window.upWorld);
    if (!forward.has_value() || !right.has_value() || !up.has_value())
    {
        return false;
    }
    return std::fabs(Dot(*forward, *right)) <= kMaximumOrthogonalityError &&
        std::fabs(Dot(*forward, *up)) <= kMaximumOrthogonalityError &&
        std::fabs(Dot(*right, *up)) <= kMaximumOrthogonalityError;
}

float SmoothFalloff(const float excess, const float feather) noexcept
{
    if (feather <= 0.0F)
    {
        return 1.0F;
    }
    const float t = std::clamp(excess / feather, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

} // namespace

std::optional<MountedCabinMaskSample> EvaluateMountedCabinMask(
    const Vec3& viewForwardWorld,
    const MountedCabinWindow& window) noexcept
{
    if (!IsFinite(viewForwardWorld) || !IsValidWindow(window))
    {
        return std::nullopt;
    }
    const auto viewForward = Normalize(viewForwardWorld);
    const auto forward = Normalize(window.forwardWorld);
    const auto right = Normalize(window.rightWorld);
    const auto up = Normalize(window.upWorld);
    if (!viewForward.has_value() || !forward.has_value() ||
        !right.has_value() || !up.has_value())
    {
        return std::nullopt;
    }

    const float localRight = Dot(*viewForward, *right);
    const float localUp = Dot(*viewForward, *up);
    const float localForward = Dot(*viewForward, *forward);

    MountedCabinMaskSample sample = {};
    sample.valid = true;
    if (localForward <= kMinimumForwardComponent)
    {
        sample.horizontalRadians = localRight < 0.0F ? -kPi : kPi;
        sample.verticalRadians = localUp < 0.0F ? -kPi : kPi;
        sample.inside = false;
        sample.alpha = 1.0F;
        return sample;
    }

    sample.horizontalRadians = std::atan2(localRight, localForward);
    sample.verticalRadians = std::atan2(localUp, localForward);
    const float horizontalExcess =
        std::fabs(sample.horizontalRadians) -
        window.halfHorizontalRadians;
    const float verticalExcess =
        std::fabs(sample.verticalRadians) -
        window.halfVerticalRadians;
    sample.inside = horizontalExcess <= 0.0F && verticalExcess <= 0.0F;
    const float horizontalAlpha = horizontalExcess <= 0.0F
        ? 0.0F
        : SmoothFalloff(horizontalExcess, window.featherRadians);
    const float verticalAlpha = verticalExcess <= 0.0F
        ? 0.0F
        : SmoothFalloff(verticalExcess, window.featherRadians);
    sample.alpha = std::max(horizontalAlpha, verticalAlpha);
    return sample;
}

std::optional<Vec3> ClampForwardToMountedCabinWindow(
    const Vec3& viewForwardWorld,
    const MountedCabinWindow& window) noexcept
{
    const auto sample = EvaluateMountedCabinMask(viewForwardWorld, window);
    if (!sample.has_value())
    {
        return std::nullopt;
    }
    if (sample->inside)
    {
        return Normalize(viewForwardWorld);
    }

    const float horizontal = std::clamp(
        sample->horizontalRadians,
        -window.halfHorizontalRadians,
        window.halfHorizontalRadians);
    const float vertical = std::clamp(
        sample->verticalRadians,
        -window.halfVerticalRadians,
        window.halfVerticalRadians);
    const auto forward = Normalize(window.forwardWorld);
    const auto right = Normalize(window.rightWorld);
    const auto up = Normalize(window.upWorld);
    if (!forward.has_value() || !right.has_value() || !up.has_value())
    {
        return std::nullopt;
    }
    const Vec3 clamped = {
        forward->x + right->x * std::tan(horizontal) +
            up->x * std::tan(vertical),
        forward->y + right->y * std::tan(horizontal) +
            up->y * std::tan(vertical),
        forward->z + right->z * std::tan(horizontal) +
            up->z * std::tan(vertical)};
    return Normalize(clamped);
}

} // namespace bfvr::stereo
