#pragma once

#include "stereo/StereoMath.h"

#include <optional>

namespace bfvr::stereo
{

inline constexpr float kUnrestrictedOffHandWeaponSwingRadians =
    3.14159265358979323846F;

struct OffHandWeaponSteeringResult
{
    Matrix4 gunWorld = {};
    float requestedSwingRadians = 0.0F;
    float appliedSwingRadians = 0.0F;
};

// Rotates a right-authoritative gun basis minimally about its fixed
// translation pivot so the predicted authored support direction approaches
// the tracked left hand. Radial mismatch is ignored and no scale is applied.
// Steering fades out when both hand spans fall below a full foregrip distance,
// so a close two-hand grip cannot amplify positional micro-motion into a large
// swing. The applied swing is capped at 120 degrees so the response is
// monotonic (no reverse correction as the hand moves further); a support
// direction at or beyond 150 degrees is rejected fail-closed because a
// near-opposite hand makes the minimal rotation axis unstable and can flip the
// weapon up or down. Degenerate and non-finite spans fail closed.
[[nodiscard]] std::optional<OffHandWeaponSteeringResult>
ComputeBoundedOffHandWeaponSteering(
    const Matrix4& controllerGunWorld,
    const Matrix4& predictedSupportWorld,
    const Matrix4& trackedLeftHandWorld,
    float maximumSwingRadians,
    float worldUnitsPerMetre) noexcept;

} // namespace bfvr::stereo
