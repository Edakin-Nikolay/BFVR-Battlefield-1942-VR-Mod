#include "stereo/MountedCameraMath.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
using bfvr::stereo::Matrix4;
using bfvr::stereo::Vec3;
using bfvr::stereo::Vec4;

bool IsFinite(const Matrix4& matrix) noexcept
{
    for (const auto& row : matrix.values)
    {
        for (const float value : row)
        {
            if (!std::isfinite(value))
            {
                return false;
            }
        }
    }
    return true;
}

bool IsFinite(const Vec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

// Copy of a rigid transform with its translation row cleared, so a row-vector
// product applies only its orientation.
Matrix4 RotationOnly(const Matrix4& matrix) noexcept
{
    Matrix4 result = matrix;
    for (std::size_t column = 0; column < 4; ++column)
    {
        result.values[3][column] = column == 3 ? 1.0F : 0.0F;
    }
    return result;
}

bool IsRigidAffine(const Matrix4& matrix) noexcept
{
    constexpr float kAffineTolerance = 0.001F;
    constexpr float kOrthonormalTolerance = 0.01F;
    if (!IsFinite(matrix) ||
        std::fabs(matrix.values[0][3]) > kAffineTolerance ||
        std::fabs(matrix.values[1][3]) > kAffineTolerance ||
        std::fabs(matrix.values[2][3]) > kAffineTolerance ||
        std::fabs(matrix.values[3][3] - 1.0F) > kAffineTolerance)
    {
        return false;
    }

    for (std::size_t row = 0; row < 3; ++row)
    {
        float lengthSquared = 0.0F;
        for (std::size_t column = 0; column < 3; ++column)
        {
            lengthSquared +=
                matrix.values[row][column] * matrix.values[row][column];
        }
        if (std::fabs(lengthSquared - 1.0F) > kOrthonormalTolerance)
        {
            return false;
        }
        for (std::size_t other = row + 1; other < 3; ++other)
        {
            float dot = 0.0F;
            for (std::size_t column = 0; column < 3; ++column)
            {
                dot += matrix.values[row][column] *
                    matrix.values[other][column];
            }
            if (std::fabs(dot) > kOrthonormalTolerance)
            {
                return false;
            }
        }
    }
    return true;
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

std::optional<Matrix4> Invert(const Matrix4& matrix) noexcept
{
    if (!IsRigidAffine(matrix))
    {
        return std::nullopt;
    }

    // For a rigid row-vector transform [R 0; t 1], the inverse is
    // [transpose(R) 0; -t*transpose(R) 1].
    Matrix4 inverse = {};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            inverse.values[row][column] = matrix.values[column][row];
        }
    }
    for (std::size_t column = 0; column < 3; ++column)
    {
        for (std::size_t inner = 0; inner < 3; ++inner)
        {
            inverse.values[3][column] -=
                matrix.values[3][inner] * inverse.values[inner][column];
        }
    }
    inverse.values[3][3] = 1.0F;
    return IsRigidAffine(inverse)
        ? std::optional<Matrix4>(inverse)
        : std::nullopt;
}
} // namespace

namespace bfvr::stereo
{
std::optional<Matrix4> CaptureD3D8MountedCameraAnchor(
    const Matrix4& sourceCameraWorld,
    const Matrix4& stationWorld) noexcept
{
    if (!IsRigidAffine(sourceCameraWorld))
    {
        return std::nullopt;
    }
    const auto inverseStation = Invert(stationWorld);
    if (!inverseStation.has_value())
    {
        return std::nullopt;
    }
    const Matrix4 cameraInStation =
        Multiply(sourceCameraWorld, *inverseStation);
    return IsRigidAffine(cameraInStation)
        ? std::optional<Matrix4>(cameraInStation)
        : std::nullopt;
}

std::optional<Matrix4> ComposeD3D8MountedCameraFromAnchor(
    const Matrix4& cameraInStation,
    const Matrix4& stationWorld) noexcept
{
    if (!IsRigidAffine(cameraInStation) || !IsRigidAffine(stationWorld))
    {
        return std::nullopt;
    }
    const Matrix4 cameraWorld = Multiply(cameraInStation, stationWorld);
    return IsRigidAffine(cameraWorld)
        ? std::optional<Matrix4>(cameraWorld)
        : std::nullopt;
}

std::optional<Vec3> ClampStationLocalOffset(
    const Vec3& offsetInStation,
    const MountedStationVolume& volume) noexcept
{
    if (!IsFinite(offsetInStation) ||
        !std::isfinite(volume.halfWidth) ||
        !std::isfinite(volume.halfHeight) ||
        !std::isfinite(volume.halfDepth) ||
        volume.halfWidth <= 0.0F ||
        volume.halfHeight <= 0.0F ||
        volume.halfDepth <= 0.0F)
    {
        return std::nullopt;
    }

    return Vec3{
        std::clamp(offsetInStation.x, -volume.halfWidth, volume.halfWidth),
        std::clamp(offsetInStation.y, -volume.halfHeight, volume.halfHeight),
        std::clamp(offsetInStation.z, -volume.halfDepth, volume.halfDepth)};
}

const MountedStationVolume& SelectMountedStationVolume(
    MountedStationClass stationClass) noexcept
{
    switch (stationClass)
    {
        case MountedStationClass::Air:
            return kAircraftStationVolume;
        case MountedStationClass::Land:
            return kTankStationVolume;
        case MountedStationClass::Unknown:
        default:
            return kDefaultMountedStationVolume;
    }
}

std::optional<Matrix4> ClampD3D8HeadOffsetToStationVolume(
    const Matrix4& composedCameraWorld,
    const Matrix4& cameraInStation,
    const Matrix4& stationWorld,
    const MountedStationVolume& volume) noexcept
{
    if (!IsRigidAffine(composedCameraWorld) ||
        !IsRigidAffine(cameraInStation) ||
        !IsRigidAffine(stationWorld))
    {
        return std::nullopt;
    }

    // Row-vector convention maps a world-space point through the object
    // transform, so a world-space direction enters the station's own axes
    // through the transpose of the station rotation.
    const auto inverseStation = Invert(stationWorld);
    if (!inverseStation.has_value())
    {
        return std::nullopt;
    }

    const Matrix4 anchorWorld = Multiply(cameraInStation, stationWorld);
    if (!IsFinite(anchorWorld))
    {
        return std::nullopt;
    }

    const Vec4 headOffsetWorld{
        composedCameraWorld.values[3][0] - anchorWorld.values[3][0],
        composedCameraWorld.values[3][1] - anchorWorld.values[3][1],
        composedCameraWorld.values[3][2] - anchorWorld.values[3][2],
        0.0F};
    const Vec4 offsetInStation = bfvr::stereo::TransformRowVector(
        headOffsetWorld,
        RotationOnly(*inverseStation));

    const auto clamped = ClampStationLocalOffset(
        {offsetInStation.x, offsetInStation.y, offsetInStation.z},
        volume);
    if (!clamped.has_value())
    {
        return std::nullopt;
    }

    const Vec4 clampedOffsetWorld = bfvr::stereo::TransformRowVector(
        {clamped->x, clamped->y, clamped->z, 0.0F},
        RotationOnly(stationWorld));

    Matrix4 result = composedCameraWorld;
    result.values[3][0] = anchorWorld.values[3][0] + clampedOffsetWorld.x;
    result.values[3][1] = anchorWorld.values[3][1] + clampedOffsetWorld.y;
    result.values[3][2] = anchorWorld.values[3][2] + clampedOffsetWorld.z;
    return IsRigidAffine(result)
        ? std::optional<Matrix4>(result)
        : std::nullopt;
}

float SelectD3D8VisibilityFrustumVerticalScale(
    bool mountedCameraDecoupled) noexcept
{
    // 1.50 is the owner-accepted ordinary/scoped VR margin. Decoupled mounted
    // cameras can sit appreciably above nearby soldier object origins, whose
    // body and attached weapon are admitted through separate culling nodes.
    // 2.00 expands only top/bottom coverage for that state; callers preserve
    // the horizontal half-angle by scaling aspect by the same factor.
    return mountedCameraDecoupled ? 2.00F : 1.50F;
}

MountedCameraControlTransition UpdateMountedCameraControl(
    MountedCameraControlState& state,
    std::uintptr_t stationIdentity,
    std::uint32_t toggleSequence) noexcept
{
    MountedCameraControlTransition result = {};
    std::uint32_t newEdges = 0;
    if (toggleSequence >= state.lastToggleSequence)
    {
        newEdges = toggleSequence - state.lastToggleSequence;
        state.lastToggleSequence = toggleSequence;
    }

    if (state.stationIdentity != stationIdentity)
    {
        result.stationChanged = true;
        state.stationIdentity = stationIdentity;
        if (state.decoupled)
        {
            state.decoupled = false;
            result.decouplingChanged = true;
        }
    }

    if (stationIdentity == 0)
    {
        result.toggleIgnored = newEdges != 0;
        return result;
    }
    if ((newEdges & 1U) != 0)
    {
        state.decoupled = !state.decoupled;
        result.decouplingChanged = true;
        result.toggleApplied = true;
    }
    return result;
}
} // namespace bfvr::stereo
