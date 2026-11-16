#include "client/BFSoldierVrArmTracking.h"

#include "client/ControllerInputCache.h"
#include "client/D3D8TrackingAnchor.h"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace bfvr
{
namespace
{
constexpr LONG kStandingPose = 0;
constexpr LONG kLastSupportedPose = 2;
constexpr float kMaximumPoseCameraTranslation = 3.0F;
using Matrix4 = native_arm_math::Matrix4;
using GetTransformationFn = const Matrix4*(__thiscall*)(void*);
using GetSoldierPoseFn = LONG(__thiscall*)(void*);
using GetPoseCameraPositionFn = const float*(__thiscall*)(void*, LONG);
}

std::optional<Matrix4> ReadBf1942ObjectTransform(void* object) noexcept
{
    if (object == nullptr)
    {
        return std::nullopt;
    }
    __try
    {
        void* const vtable = *reinterpret_cast<void* const*>(object);
        void* const target = vtable == nullptr
            ? nullptr
            : *reinterpret_cast<void* const*>(
                static_cast<const std::byte*>(vtable) + 0x3C);
        const auto getter = reinterpret_cast<GetTransformationFn>(target);
        const Matrix4* const matrix =
            getter == nullptr ? nullptr : getter(object);
        if (matrix == nullptr)
        {
            return std::nullopt;
        }
        Matrix4 copy = {};
        std::memcpy(&copy, matrix, sizeof(copy));
        return native_arm_math::IsFinite(copy)
            ? std::optional<Matrix4>(copy)
            : std::nullopt;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return std::nullopt;
    }
}

std::optional<BFSoldierVrPoseCameraTranslation>
ReadBFSoldierVrPoseCameraTranslation(
    void* soldier,
    void* getSoldierPoseTarget,
    void* getPoseCameraPositionTarget) noexcept
{
    if (soldier == nullptr || getSoldierPoseTarget == nullptr ||
        getPoseCameraPositionTarget == nullptr)
    {
        return std::nullopt;
    }
    __try
    {
        const auto getPose =
            reinterpret_cast<GetSoldierPoseFn>(getSoldierPoseTarget);
        const auto getPoseCameraPosition =
            reinterpret_cast<GetPoseCameraPositionFn>(
                getPoseCameraPositionTarget);
        const LONG pose = getPose(soldier);
        if (pose < kStandingPose || pose > kLastSupportedPose)
        {
            return std::nullopt;
        }
        const float* const standing =
            getPoseCameraPosition(soldier, kStandingPose);
        const float* const current =
            getPoseCameraPosition(soldier, pose);
        if (standing == nullptr || current == nullptr)
        {
            return std::nullopt;
        }
        BFSoldierVrPoseCameraTranslation result = {};
        result.pose = pose;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            result.localDelta[axis] = current[axis] - standing[axis];
            if (!std::isfinite(result.localDelta[axis]) ||
                std::fabs(result.localDelta[axis]) >
                    kMaximumPoseCameraTranslation)
            {
                return std::nullopt;
            }
        }
        return result;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return std::nullopt;
    }
}

bool ReadFreshBFSoldierVrArmTracking(
    const native_arm_math::Matrix4& soldierTransform,
    const DWORD maximumAgeMs,
    BFSoldierVrArmTracking& result) noexcept
{
    result = {};
    float observedBodyYaw = 0.0F;
    bool observedBodyYawValid = false;
    float presentationYaw = 0.0F;
    bool presentationYawValid = false;
    if (!ReadFreshAcceptedInfantryPresentationInput(
            result.controllers,
            observedBodyYaw,
            observedBodyYawValid,
            presentationYaw,
            presentationYawValid,
            result.generation,
            maximumAgeMs))
    {
        return false;
    }

    D3D8RuntimeControllerSample simultaneousSample = {};
    D3D8RuntimeView matchingHead = {};
    LONG headGeneration = 0;
    if (!ReadFreshAcceptedWeaponTracking(
            simultaneousSample,
            matchingHead,
            headGeneration,
            maximumAgeMs) ||
        headGeneration != result.generation)
    {
        return false;
    }

    if (observedBodyYawValid)
    {
        result.observedBodyYawRadians = observedBodyYaw;
        result.observedBodyYawValid = true;
        const auto currentBodyYaw =
            native_arm_math::ExtractBodyYaw(soldierTransform);
        if (!currentBodyYaw.has_value())
        {
            return false;
        }
        D3D8RuntimeControllerSample adjusted = {};
        D3D8RuntimeControllerSample headCarrier = {};
        headCarrier.hands[0].gripPose = {
            matchingHead.orientationX,
            matchingHead.orientationY,
            matchingHead.orientationZ,
            matchingHead.orientationW,
            matchingHead.positionX,
            matchingHead.positionY,
            matchingHead.positionZ};
        D3D8RuntimeControllerSample adjustedHead = {};
        // Compensate the body advance minus the presentation advance between
        // publication and this callback. Using body alone leaves a residual
        // proportional to turn speed while the artificial turn is active.
        float livePresentationYaw = 0.0F;
        bool livePresentationValid = false;
        (void)bfvr::ReadLiveInfantryPresentationYaw(
            livePresentationYaw, livePresentationValid);
        float rebaseSourceYaw = observedBodyYaw;
        float rebaseTargetYaw = *currentBodyYaw;
        if (livePresentationValid && presentationYawValid)
        {
            const float bodyDelta = *currentBodyYaw - observedBodyYaw;
            const float presentationDelta =
                livePresentationYaw - presentationYaw;
            rebaseSourceYaw = 0.0F;
            rebaseTargetYaw = bodyDelta - presentationDelta;
        }
        if (!RebaseInfantryControllerSampleToCurrentBodyYaw(
                result.controllers,
                rebaseSourceYaw,
                rebaseTargetYaw,
                adjusted) ||
            !RebaseInfantryControllerSampleToCurrentBodyYaw(
                headCarrier,
                rebaseSourceYaw,
                rebaseTargetYaw,
                adjustedHead))
        {
            return false;
        }
        result.controllers = adjusted;
        const auto& rebased = adjustedHead.hands[0].gripPose;
        matchingHead.positionX = rebased.positionX;
        matchingHead.positionY = rebased.positionY;
        matchingHead.positionZ = rebased.positionZ;
    }

    result.headSkeletonPosition = {
        matchingHead.positionX,
        matchingHead.positionY,
        -matchingHead.positionZ};
    result.presentationYawRadians = presentationYaw;
    result.presentationYawValid = presentationYawValid;
    return std::isfinite(result.headSkeletonPosition[0]) &&
        std::isfinite(result.headSkeletonPosition[1]) &&
        std::isfinite(result.headSkeletonPosition[2]);
}

} // namespace bfvr
