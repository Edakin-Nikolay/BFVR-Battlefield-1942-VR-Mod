#pragma once

#include "stereo/StereoMath.h"

#include <optional>

namespace bfvr::stereo
{

// Interior window of a mounted station as seen from the native flat camera.
// The orientation is the station/native camera basis (forward points through
// the vision slit); the half angles describe the visible opening. BFVR has no
// measured hull geometry, so these values remain named calibration inputs.
struct MountedCabinWindow
{
    Vec3 forwardWorld = {0.0F, 0.0F, 1.0F};
    Vec3 rightWorld = {1.0F, 0.0F, 0.0F};
    Vec3 upWorld = {0.0F, 1.0F, 0.0F};
    float halfHorizontalRadians = 0.0F;
    float halfVerticalRadians = 0.0F;
    float featherRadians = 0.0F;
};

struct MountedCabinMaskSample
{
    bool valid = false;
    bool inside = false;
    float horizontalRadians = 0.0F;
    float verticalRadians = 0.0F;
    // 0 = clear window, 1 = fully dark. The transition band is
    // featherRadians wide outside each half angle.
    float alpha = 1.0F;
};

// Measures a normalized world-space view direction against the native slit
// window. Non-finite or non-orthonormal inputs fail closed with no value.
[[nodiscard]] std::optional<MountedCabinMaskSample> EvaluateMountedCabinMask(
    const Vec3& viewForwardWorld,
    const MountedCabinWindow& window) noexcept;

// Fallback policy: clamps a world-space view direction into the same angular
// window. A direction already inside is returned unchanged (normalized).
// Orientation is rebuilt from the window basis, so this helper intentionally
// does not preserve view roll.
[[nodiscard]] std::optional<Vec3> ClampForwardToMountedCabinWindow(
    const Vec3& viewForwardWorld,
    const MountedCabinWindow& window) noexcept;

} // namespace bfvr::stereo
