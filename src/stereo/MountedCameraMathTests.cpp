#include "stereo/MountedCameraMath.h"

#include <cmath>
#include <cstdio>

namespace
{
using bfvr::stereo::Matrix4;
using bfvr::stereo::MountedStationVolume;
using bfvr::stereo::Vec3;

constexpr float kTolerance = 0.0001F;
constexpr float kHalfPi = 1.57079632679F;

Matrix4 Identity() noexcept
{
    Matrix4 result = {};
    for (std::size_t index = 0; index < 4; ++index)
    {
        result.values[index][index] = 1.0F;
    }
    return result;
}

Matrix4 Translation(float x, float y, float z) noexcept
{
    Matrix4 result = Identity();
    result.values[3][0] = x;
    result.values[3][1] = y;
    result.values[3][2] = z;
    return result;
}

Matrix4 Yaw(float radians) noexcept
{
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][2] = -sine;
    result.values[2][0] = sine;
    result.values[2][2] = cosine;
    return result;
}

Matrix4 Multiply(const Matrix4& left, const Matrix4& right) noexcept
{
    Matrix4 result = {};
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            for (std::size_t inner = 0; inner < 4; ++inner)
            {
                result.values[row][column] +=
                    left.values[row][inner] * right.values[inner][column];
            }
        }
    }
    return result;
}

bool NearlyEqual(const Matrix4& first, const Matrix4& second) noexcept
{
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            if (std::fabs(
                    first.values[row][column] -
                    second.values[row][column]) > kTolerance)
            {
                return false;
            }
        }
    }
    return true;
}

bool NearlyEqual(const Vec3& first, const Vec3& second) noexcept
{
    return std::fabs(first.x - second.x) <= kTolerance &&
        std::fabs(first.y - second.y) <= kTolerance &&
        std::fabs(first.z - second.z) <= kTolerance;
}

// A rigid transform with a yaw rotation block and a translation row, in the
// D3D8 row-vector convention.
Matrix4 YawAt(float radians, float x, float y, float z) noexcept
{
    Matrix4 result = Yaw(radians);
    result.values[3][0] = x;
    result.values[3][1] = y;
    result.values[3][2] = z;
    return result;
}

Vec3 TranslationRow(const Matrix4& matrix) noexcept
{
    return {matrix.values[3][0], matrix.values[3][1], matrix.values[3][2]};
}

bool TestStationMotionSurvivesAndLaterGunAimDoesNot() noexcept
{
    const Matrix4 stationAtCapture = Translation(10.0F, 2.0F, -4.0F);
    const Matrix4 cameraInStation = Multiply(
        Translation(0.0F, 1.4F, -0.5F),
        Yaw(0.35F));
    const Matrix4 sourceAtCapture =
        Multiply(cameraInStation, stationAtCapture);
    const auto anchor = bfvr::stereo::CaptureD3D8MountedCameraAnchor(
        sourceAtCapture,
        stationAtCapture);
    if (!anchor.has_value() || !NearlyEqual(*anchor, cameraInStation))
    {
        return false;
    }

    const Matrix4 movedStation = Multiply(
        Yaw(-0.6F),
        Translation(14.0F, 2.5F, 9.0F));
    const Matrix4 expected = Multiply(cameraInStation, movedStation);
    const auto decoupled =
        bfvr::stereo::ComposeD3D8MountedCameraFromAnchor(
            *anchor,
            movedStation);

    // This is the native camera if a child turret yaws after capture. It must
    // differ from the reconstructed station-relative camera.
    const Matrix4 nativeAimed = Multiply(
        Multiply(cameraInStation, Yaw(kHalfPi)),
        movedStation);
    return decoupled.has_value() && NearlyEqual(*decoupled, expected) &&
        !NearlyEqual(*decoupled, nativeAimed);
}

bool TestGunPivotTranslationIsNotInherited() noexcept
{
    const Matrix4 station = Identity();
    const Matrix4 cameraInStation = Translation(0.0F, 1.2F, -0.8F);
    const auto anchor = bfvr::stereo::CaptureD3D8MountedCameraAnchor(
        cameraInStation,
        station);
    if (!anchor.has_value())
    {
        return false;
    }
    const Matrix4 nativeAimed = Multiply(cameraInStation, Yaw(kHalfPi));
    const auto decoupled =
        bfvr::stereo::ComposeD3D8MountedCameraFromAnchor(*anchor, station);
    return decoupled.has_value() &&
        NearlyEqual(*decoupled, cameraInStation) &&
        !NearlyEqual(*decoupled, nativeAimed);
}

bool TestInvalidTransformsFailClosed() noexcept
{
    Matrix4 invalid = Identity();
    invalid.values[0][0] = 0.0F;
    return !bfvr::stereo::CaptureD3D8MountedCameraAnchor(
                Identity(),
                invalid).has_value() &&
        !bfvr::stereo::ComposeD3D8MountedCameraFromAnchor(
                invalid,
                Identity()).has_value();
}

bool TestDefaultCoupledSeatLocalTogglePolicy() noexcept
{
    bfvr::stereo::MountedCameraControlState state = {};
    auto transition = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x1000,
        0);
    if (!transition.stationChanged || state.decoupled)
    {
        return false;
    }
    transition = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x1000,
        1);
    if (!transition.toggleApplied || !transition.decouplingChanged ||
        !state.decoupled)
    {
        return false;
    }
    transition = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x1000,
        2);
    if (!transition.toggleApplied || state.decoupled)
    {
        return false;
    }
    (void)bfvr::stereo::UpdateMountedCameraControl(state, 0x1000, 3);
    transition = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x2000,
        3);
    if (!transition.stationChanged || !transition.decouplingChanged ||
        state.decoupled)
    {
        return false;
    }
    transition = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x2000,
        4);
    if (!transition.toggleApplied || !state.decoupled)
    {
        return false;
    }
    transition = bfvr::stereo::UpdateMountedCameraControl(state, 0, 4);
    return transition.stationChanged && transition.decouplingChanged &&
        !state.decoupled;
}

bool TestToggleOutsideStationIsConsumedAndIgnored() noexcept
{
    bfvr::stereo::MountedCameraControlState state = {};
    const auto ignored = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0,
        1);
    const auto entered = bfvr::stereo::UpdateMountedCameraControl(
        state,
        0x3000,
        1);
    return ignored.toggleIgnored && !state.decoupled &&
        entered.stationChanged && !entered.toggleApplied;
}

bool TestMountedVisibilityMarginIsStateLocal() noexcept
{
    return std::fabs(
               bfvr::stereo::SelectD3D8VisibilityFrustumVerticalScale(false) -
               1.50F) <= kTolerance &&
        std::fabs(
            bfvr::stereo::SelectD3D8VisibilityFrustumVerticalScale(true) -
            2.00F) <= kTolerance;
}

// Composes the runtime head path over a station anchor, exactly as the
// presentation hook does, so the tests measure the same camera the game draws.
Matrix4 ComposedHeadCamera(
    const Matrix4& cameraInStation,
    const Matrix4& stationWorld,
    const Matrix4& relativeHeadCamera) noexcept
{
    return Multiply(relativeHeadCamera, Multiply(cameraInStation, stationWorld));
}

// Inside the interior volume the clamped camera must reproduce the existing
// unclamped head composition exactly, including in a yawed station frame.
bool TestClampedHeadInsideVolumeMatchesUnclampedComposition() noexcept
{
    const Matrix4 cameraInStation = Translation(0.50F, 0.00F, 0.00F);
    const Matrix4 stationWorld = YawAt(0.30F, 7.0F, 1.0F, 3.0F);
    const Matrix4 relativeHeadCamera = YawAt(0.35F, 0.30F, 0.10F, 0.20F);
    const Matrix4 composed = ComposedHeadCamera(
        cameraInStation,
        stationWorld,
        relativeHeadCamera);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        composed,
        cameraInStation,
        stationWorld,
        bfvr::stereo::kTankStationVolume);
    return clamped.has_value() && NearlyEqual(*clamped, composed);
}

// A head outside the volume is moved to the nearest boundary of the volume.
bool TestClampedHeadOutsideVolumeSnapsToNearestBoundary() noexcept
{
    const Matrix4 cameraInStation = Translation(0.00F, 1.20F, -0.80F);
    const Matrix4 relativeHeadCamera = YawAt(0.00F, 1.00F, -0.50F, 0.90F);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        ComposedHeadCamera(
            cameraInStation,
            Identity(),
            relativeHeadCamera),
        cameraInStation,
        Identity(),
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    const Vec3 expected{0.45F, 0.90F, -0.20F};
    return NearlyEqual(TranslationRow(*clamped), expected);
}

// Only the axes that leave the volume are reduced; an axis already inside the
// volume is untouched, so a head that fits on one axis is not moved.
bool TestClampReducesOnlyTheAxisThatLeavesTheVolume() noexcept
{
    const Matrix4 cameraInStation = Identity();
    const Matrix4 relativeHeadCamera = YawAt(0.00F, 0.10F, 1.00F, 0.20F);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        ComposedHeadCamera(
            cameraInStation,
            Identity(),
            relativeHeadCamera),
        cameraInStation,
        Identity(),
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    const Vec3 expected{0.10F, 0.30F, 0.20F};
    return NearlyEqual(TranslationRow(*clamped), expected);
}

// The clamped head offset never exceeds the half-extents in any axis.
bool TestClampedHeadNeverExceedsHalfExtents() noexcept
{
    const Matrix4 cameraInStation = Translation(0.60F, 0.20F, -0.30F);
    const Matrix4 relativeHeadCamera = YawAt(0.00F, 5.00F, 2.00F, 4.00F);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        ComposedHeadCamera(
            cameraInStation,
            Identity(),
            relativeHeadCamera),
        cameraInStation,
        Identity(),
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    const Vec3 offsetInStation{
        clamped->values[3][0] - cameraInStation.values[3][0],
        clamped->values[3][1] - cameraInStation.values[3][1],
        clamped->values[3][2] - cameraInStation.values[3][2]};
    return std::fabs(offsetInStation.x) <=
            bfvr::stereo::kTankStationVolume.halfWidth + kTolerance &&
        std::fabs(offsetInStation.y) <=
            bfvr::stereo::kTankStationVolume.halfHeight + kTolerance &&
        std::fabs(offsetInStation.z) <=
            bfvr::stereo::kTankStationVolume.halfDepth + kTolerance &&
        NearlyEqual(offsetInStation, Vec3{
            bfvr::stereo::kTankStationVolume.halfWidth,
            bfvr::stereo::kTankStationVolume.halfHeight,
            bfvr::stereo::kTankStationVolume.halfDepth});
}

// The clamp reduces position only; the head orientation is untouched, including
// when the clamp actually moves the head.
bool TestClampPreservesRotationExactly() noexcept
{
    const Matrix4 relativeHeadCamera = YawAt(0.40F, 5.00F, 2.00F, 4.00F);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        ComposedHeadCamera(
            Identity(),
            Identity(),
            relativeHeadCamera),
        Identity(),
        Identity(),
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            if (std::fabs(
                    clamped->values[row][column] -
                    relativeHeadCamera.values[row][column]) > kTolerance)
            {
                return false;
            }
        }
    }
    return NearlyEqual(TranslationRow(*clamped), Vec3{
        bfvr::stereo::kTankStationVolume.halfWidth,
        bfvr::stereo::kTankStationVolume.halfHeight,
        bfvr::stereo::kTankStationVolume.halfDepth});
}

// The head offset is measured in the hull's own axes, so a station-local head
// offset is clamped along the axis it actually crosses and then mapped back to
// world through the station rotation. A station yawed by pi/2 maps station-local
// +Z to world +X, so a station-local offset exceeding half depth is reduced to
// half depth and appears along world +X.
bool TestClampMapsHeadOffsetThroughStationYaw() noexcept
{
    const Matrix4 stationWorld = YawAt(kHalfPi, 10.0F, 0.0F, 5.0F);
    const Matrix4 relativeHeadCamera = YawAt(0.00F, 0.00F, 0.00F, 1.00F);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        ComposedHeadCamera(
            Identity(),
            stationWorld,
            relativeHeadCamera),
        Identity(),
        stationWorld,
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    // Station-local +Z is world +X for this yaw, so the clamped head sits at
    // half depth from the anchor along world +X.
    const Vec3 expected{10.00F + bfvr::stereo::kTankStationVolume.halfDepth,
        0.00F, 5.00F};
    return NearlyEqual(TranslationRow(*clamped), expected);
}

// A station-local offset already inside the volume is not affected by the
// station's yaw: the mapped position equals the unclamped composition.
bool TestInsideOffsetIsUnaffectedByStationYaw() noexcept
{
    const Matrix4 stationWorld = YawAt(kHalfPi, 10.0F, 0.0F, 5.0F);
    const Matrix4 relativeHeadCamera = YawAt(0.00F, 0.20F, 0.10F, 0.20F);
    const Matrix4 composed = ComposedHeadCamera(
        Identity(),
        stationWorld,
        relativeHeadCamera);
    const auto clamped = bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        composed,
        Identity(),
        stationWorld,
        bfvr::stereo::kTankStationVolume);
    if (!clamped.has_value())
    {
        return false;
    }
    // Station-local +X is world -Z and station-local +Z is world +X for this
    // yaw, so the world position is the anchor plus the unchanged offset.
    const Vec3 expected{10.20F, 0.10F, 4.80F};
    return NearlyEqual(TranslationRow(*clamped), expected) &&
        NearlyEqual(*clamped, composed);
}

// Invalid offsets, degenerate volumes, and non-rigid inputs fail closed.
bool TestClampFailClosedOnInvalidInputs() noexcept
{
    const float nan = std::nanf("");
    if (bfvr::stereo::ClampStationLocalOffset(
            Vec3{nan, 0.0F, 0.0F},
            bfvr::stereo::kTankStationVolume).has_value())
    {
        return false;
    }
    const auto insideVolume = bfvr::stereo::ClampStationLocalOffset(
        Vec3{0.10F, 0.10F, 0.10F},
        bfvr::stereo::kTankStationVolume);
    if (!insideVolume.has_value() ||
        !NearlyEqual(*insideVolume, Vec3{0.10F, 0.10F, 0.10F}))
    {
        return false;
    }
    if (bfvr::stereo::ClampStationLocalOffset(
            Vec3{0.10F, 0.10F, 0.10F},
            {0.00F, 0.30F, 0.60F}).has_value())
    {
        return false;
    }
    if (bfvr::stereo::ClampStationLocalOffset(
            Vec3{0.10F, 0.10F, 0.10F},
            {0.45F, -0.30F, 0.60F}).has_value())
    {
        return false;
    }
    Matrix4 invalid = Identity();
    invalid.values[0][0] = 0.0F;
    if (bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
            invalid,
            Identity(),
            Identity(),
            bfvr::stereo::kTankStationVolume).has_value())
    {
        return false;
    }
    Matrix4 nonFinite = Identity();
    nonFinite.values[3][1] = nan;
    if (bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
            nonFinite,
            Identity(),
            Identity(),
            bfvr::stereo::kTankStationVolume).has_value())
    {
        return false;
    }
    Matrix4 nonRigidStation = Identity();
    nonRigidStation.values[2][2] = 0.0F;
    if (bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
            Identity(),
            Identity(),
            nonRigidStation,
            bfvr::stereo::kTankStationVolume).has_value())
    {
        return false;
    }
    return !bfvr::stereo::ClampD3D8HeadOffsetToStationVolume(
        Identity(),
        Identity(),
        Identity(),
        {0.45F, 0.30F, -0.60F}).has_value();
}

// The volume policy resolves to the documented envelopes, and an unavailable
// vehicle class keeps the default (larger) envelope rather than a tighter one,
// so an unknown station clamps the least.
bool TestVolumePolicySelectsDocumentedVolumes() noexcept
{
    const MountedStationVolume& air = bfvr::stereo::SelectMountedStationVolume(
        bfvr::stereo::MountedStationClass::Air);
    const MountedStationVolume& land = bfvr::stereo::SelectMountedStationVolume(
        bfvr::stereo::MountedStationClass::Land);
    const MountedStationVolume& unknown = bfvr::stereo::SelectMountedStationVolume(
        bfvr::stereo::MountedStationClass::Unknown);
    if (air.halfWidth != bfvr::stereo::kAircraftStationVolume.halfWidth ||
        air.halfHeight != bfvr::stereo::kAircraftStationVolume.halfHeight ||
        air.halfDepth != bfvr::stereo::kAircraftStationVolume.halfDepth)
    {
        return false;
    }
    if (land.halfWidth != bfvr::stereo::kTankStationVolume.halfWidth ||
        land.halfHeight != bfvr::stereo::kTankStationVolume.halfHeight ||
        land.halfDepth != bfvr::stereo::kTankStationVolume.halfDepth)
    {
        return false;
    }
    if (unknown.halfWidth != bfvr::stereo::kDefaultMountedStationVolume.halfWidth ||
        unknown.halfHeight != bfvr::stereo::kDefaultMountedStationVolume.halfHeight ||
        unknown.halfDepth != bfvr::stereo::kDefaultMountedStationVolume.halfDepth)
    {
        return false;
    }
    // The default envelope is the larger one, so an unknown class clamps less
    // than either documented class.
    return &unknown == &bfvr::stereo::kDefaultMountedStationVolume &&
        unknown.halfWidth >= air.halfWidth &&
        unknown.halfHeight >= air.halfHeight &&
        unknown.halfDepth >= air.halfDepth &&
        &land == &bfvr::stereo::kTankStationVolume;
}
struct NamedTest
{
    const char* name;
    bool (*check)() noexcept;
};
} // namespace

int main()
{
    const NamedTest tests[] = {
        {"StationMotionSurvivesAndLaterGunAimDoesNot",
            TestStationMotionSurvivesAndLaterGunAimDoesNot},
        {"GunPivotTranslationIsNotInherited",
            TestGunPivotTranslationIsNotInherited},
        {"InvalidTransformsFailClosed", TestInvalidTransformsFailClosed},
        {"DefaultCoupledSeatLocalTogglePolicy",
            TestDefaultCoupledSeatLocalTogglePolicy},
        {"ToggleOutsideStationIsConsumedAndIgnored",
            TestToggleOutsideStationIsConsumedAndIgnored},
        {"MountedVisibilityMarginIsStateLocal",
            TestMountedVisibilityMarginIsStateLocal},
        {"ClampedHeadInsideVolumeMatchesUnclampedComposition",
            TestClampedHeadInsideVolumeMatchesUnclampedComposition},
        {"ClampedHeadOutsideVolumeSnapsToNearestBoundary",
            TestClampedHeadOutsideVolumeSnapsToNearestBoundary},
        {"ClampReducesOnlyTheAxisThatLeavesTheVolume",
            TestClampReducesOnlyTheAxisThatLeavesTheVolume},
        {"ClampedHeadNeverExceedsHalfExtents",
            TestClampedHeadNeverExceedsHalfExtents},
        {"ClampPreservesRotationExactly", TestClampPreservesRotationExactly},
        {"ClampMapsHeadOffsetThroughStationYaw",
            TestClampMapsHeadOffsetThroughStationYaw},
        {"InsideOffsetIsUnaffectedByStationYaw",
            TestInsideOffsetIsUnaffectedByStationYaw},
        {"VolumePolicySelectsDocumentedVolumes",
            TestVolumePolicySelectsDocumentedVolumes},
        {"ClampFailClosedOnInvalidInputs", TestClampFailClosedOnInvalidInputs},
    };

    bool failed = false;
    for (const NamedTest& test : tests)
    {
        if (!test.check())
        {
            std::fprintf(stderr, "FAIL: %s\n", test.name);
            failed = true;
        }
    }
    if (failed)
    {
        std::fprintf(stderr, "Mounted-camera math tests failed.\n");
        return 1;
    }
    std::printf("Mounted-camera math tests passed.\n");
    return 0;
}
