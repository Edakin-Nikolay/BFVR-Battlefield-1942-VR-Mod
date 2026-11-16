#pragma once

#include "stereo/StereoMath.h"

#include <optional>

namespace bfvr::stereo
{

// Separates the geometric factors that make a near-wall impact land away from
// the world crosshair. The crosshair reticle ray starts at the controller-gun
// origin and follows its row-2 forward; the projectile ray starts at the same
// origin plus the weapon's local barrel offset and follows the local barrel
// direction. Both are expressed in the gun's row-vector world basis.
//
// This is a measurement model, not a runtime collision claim: it intersects
// both rays with a plane perpendicular to the reticle forward at the requested
// distance and reports the lateral/vertical separation of the two impact
// points plus the angular basis difference. Spread (game-owned) and frame
// delay are deliberately NOT modelled here; they must be measured separately.
struct CloseRangeAimParallax
{
    float lateralMeters = 0.0F;
    float verticalMeters = 0.0F;
    float basisAngularDegrees = 0.0F;
};

// Returns nullopt for non-finite input, a degenerate gun basis, a barrel
// direction that does not advance toward the plane, or a non-positive distance.
[[nodiscard]] std::optional<CloseRangeAimParallax> MeasureCloseRangeAimParallax(
    const Matrix4& gunWorld,
    const Vec3& barrelOffsetLocal,
    const Vec3& barrelDirectionLocal,
    float planeDistanceMeters) noexcept;

} // namespace bfvr::stereo
