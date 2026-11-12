#pragma once

#include "stereo/StereoMath.h"

#include <cstdint>
#include <optional>

namespace bfvr::stereo
{

// Captures the game's native camera in the occupied weapon station's local
// coordinate frame. Both inputs are rigid camera/object-to-world transforms
// in BF1942's D3D8 row-vector convention.
[[nodiscard]] std::optional<Matrix4> CaptureD3D8MountedCameraAnchor(
    const Matrix4& sourceCameraWorld,
    const Matrix4& stationWorld) noexcept;

// Reconstructs a camera-to-world transform from one captured station-local
// anchor and the occupied station's current world transform. Turret children
// may continue to rotate independently without carrying this camera, while
// movement and rotation inherited by the station itself remain intact.
[[nodiscard]] std::optional<Matrix4> ComposeD3D8MountedCameraFromAnchor(
    const Matrix4& cameraInStation,
    const Matrix4& stationWorld) noexcept;

// Interior volume available to the head inside an occupied mounted station,
// measured in the station's local frame from the captured camera anchor. BFVR
// has no per-vehicle hull geometry source, so these half-extents are named
// defaults in game world units (kWorldUnitsPerMeter is 1.0).
struct MountedStationVolume
{
    float halfWidth = 0.0F;   // station-local right
    float halfHeight = 0.0F;  // station-local up
    float halfDepth = 0.0F;   // station-local forward
};

inline constexpr MountedStationVolume kTankStationVolume = {0.45F, 0.30F, 0.60F};
inline constexpr MountedStationVolume kAircraftStationVolume = {0.30F, 0.25F, 0.55F};

// Volume used while the occupied station's vehicle class is not available from
// BF1942's read-only station query. It is the larger documented envelope, so it
// clamps the least; per-class hull data would tighten it without changing the
// invariant.
inline constexpr MountedStationVolume kDefaultMountedStationVolume =
    kTankStationVolume;

// Vehicle classes an occupied mounted station can belong to. BF1942's read-only
// station query does not expose the class, so callers pass Unknown until a
// signature-backed class source exists; Unknown resolves to the default volume.
enum class MountedStationClass
{
    Unknown,
    Land,
    Air
};

// Deterministic volume policy for the head clamp. Unknown keeps the default
// envelope so no caller receives a tighter limit than the documented one.
[[nodiscard]] const MountedStationVolume& SelectMountedStationVolume(
    MountedStationClass stationClass) noexcept;

// Clamps a station-local offset measured from the camera anchor inside the
// interior volume. Fail-closed: a non-finite offset, a non-positive extent, or
// an invalid volume returns no value rather than an unclamped offset.
[[nodiscard]] std::optional<Vec3> ClampStationLocalOffset(
    const Vec3& offsetInStation,
    const MountedStationVolume& volume) noexcept;

// Limits the head contribution of an already composed presentation camera to
// the occupied station's interior volume. The captured anchor origin and the
// camera orientation are preserved; only the head offset that would leave the
// hull is reduced. A camera already inside the volume is returned unchanged, so
// the existing decoupled-mount behavior is not otherwise modified.
[[nodiscard]] std::optional<Matrix4> ClampD3D8HeadOffsetToStationVolume(
    const Matrix4& composedCameraWorld,
    const Matrix4& cameraInStation,
    const Matrix4& stationWorld,
    const MountedStationVolume& volume) noexcept;

// Selects the temporary vertical-FOV multiplier used only while BF1942 builds
// its cached visibility frustum. Ordinary VR retains the headset-accepted
// margin. A decoupled mounted camera receives a larger vertical envelope so
// nearby dynamic-object roots below an elevated turret camera are not rejected
// while their separately culled weapon children remain visible. The renderer's
// real projection is restored before either eye is drawn.
[[nodiscard]] float SelectD3D8VisibilityFrustumVerticalScale(
    bool mountedCameraDecoupled) noexcept;

struct MountedCameraControlState
{
    std::uintptr_t stationIdentity = 0;
    std::uint32_t lastToggleSequence = 0;
    bool decoupled = false;
};

struct MountedCameraControlTransition
{
    bool stationChanged = false;
    bool decouplingChanged = false;
    bool toggleApplied = false;
    bool toggleIgnored = false;
};

// Applies monotonic UI toggle edges to exactly one occupied station. A null
// station, death/exit, or any station identity change always restores native
// coupling before a later explicit edge may enable the new station.
[[nodiscard]] MountedCameraControlTransition UpdateMountedCameraControl(
    MountedCameraControlState& state,
    std::uintptr_t stationIdentity,
    std::uint32_t toggleSequence) noexcept;

} // namespace bfvr::stereo
