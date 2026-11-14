#include "client/BFSoldierArmBasisDiagnostics.h"

#include "client/D3D8RuntimeDiagnostics.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace bfvr
{
namespace
{
bool CopyMatrix(const stereo::Matrix4* source, stereo::Matrix4& result) noexcept
{
    if (source == nullptr) return false;
    __try
    {
        std::memcpy(&result, source, sizeof(result));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return native_arm_math::IsFinite(result);
}

float YawDegrees(const stereo::Matrix4& matrix) noexcept
{
    const auto yaw = native_arm_math::ExtractBodyYaw(matrix);
    return yaw.has_value() ? *yaw * 57.295779513F : NAN;
}
} // namespace

void BFSoldierArmBasisDiagnostics::Configure(
    void (*appendLog)(const wchar_t*)) noexcept
{
    appendLog_ = appendLog;
    enabled_ = appendLog != nullptr &&
        IsDeepD3D8RuntimeDiagnostics(ReadD3D8RuntimeDiagnosticLevel());
    lastReportAt_ = 0;
    reports_ = 0;
}

void BFSoldierArmBasisDiagnostics::Observe(
    const stereo::Matrix4* skeletonRoot,
    const stereo::Matrix4& inverseSoldierWorld,
    const std::array<float, 3>& rightTarget,
    const stereo::Matrix4* rightSolved,
    const std::array<float, 3>& leftTarget,
    const stereo::Matrix4* leftSolved,
    const float observedBodyYawRadians,
    const bool observedBodyYawValid,
    const float presentationYawRadians,
    const bool presentationYawValid,
    const LONG controllerGeneration,
    const void* soldier) noexcept
{
    // No timing, matrix reads or inversion in normal/off. Two reports/second,
    // at most ten minutes of samples per explicitly requested diagnostic run.
    if (!enabled_ || reports_ >= 1200) return;
    const DWORD now = GetTickCount();
    if (reports_ != 0 && now - lastReportAt_ < 500) return;
    stereo::Matrix4 root = {}, right = {}, left = {};
    if (!CopyMatrix(skeletonRoot, root) ||
        !CopyMatrix(rightSolved, right)) return;
    const auto soldierWorld = native_arm_math::Invert(inverseSoldierWorld);
    if (!soldierWorld.has_value()) return;
    const bool leftValid = CopyMatrix(leftSolved, left);
    const auto rootInSoldier = native_arm_math::Multiply(root, inverseSoldierWorld);
    const float bodyYawDeg = YawDegrees(*soldierWorld);
    const float targetYawDeg =
        (rightTarget[0] != 0.0F || rightTarget[2] != 0.0F)
            ? std::atan2(rightTarget[0], rightTarget[2]) * 57.295779513F
            : NAN;
    const float worldHandYawDeg = bodyYawDeg + targetYawDeg;
    std::array<wchar_t, 1200> message = {};
    _snwprintf_s(
        message.data(), message.size(), _TRUNCATE,
        L"ARM_BASIS generation=%ld soldier=%p rootYawDeg=%.3f bodyYawDeg=%.3f "
        L"observedBodyYawDeg=%.3f bodyYawDeltaDeg=%.3f observedBodyYawValid=%d "
        L"presentationYawDeg=%.3f presentationYawValid=%d "
        L"worldHandYawDeg=%.3f viewHandYawDeg=%.3f "
        L"rootInBodyYawDeg=%.3f rootOrigin=(%.4f,%.4f,%.4f) "
        L"bodyOrigin=(%.4f,%.4f,%.4f) "
        L"rightTarget=(%.4f,%.4f,%.4f) rightSolved=(%.4f,%.4f,%.4f) "
        L"rightError=(%.4f,%.4f,%.4f) leftValid=%d "
        L"leftTarget=(%.4f,%.4f,%.4f) leftSolved=(%.4f,%.4f,%.4f). "
        L"Targets and solved positions are native Skeleton-space candidates; "
        L"root/world ownership is diagnostic, not assumed.",
        controllerGeneration, soldier,
        YawDegrees(root), bodyYawDeg,
        observedBodyYawValid ? observedBodyYawRadians * 57.295779513F : NAN,
        observedBodyYawValid
            ? (bodyYawDeg - observedBodyYawRadians * 57.295779513F)
            : NAN,
        observedBodyYawValid ? 1 : 0,
        presentationYawValid ? presentationYawRadians * 57.295779513F : NAN,
        presentationYawValid ? 1 : 0,
        worldHandYawDeg,
        presentationYawValid
            ? worldHandYawDeg - presentationYawRadians * 57.295779513F
            : NAN,
        YawDegrees(rootInSoldier),
        root.values[3][0], root.values[3][1], root.values[3][2],
        soldierWorld->values[3][0], soldierWorld->values[3][1], soldierWorld->values[3][2],
        rightTarget[0], rightTarget[1], rightTarget[2],
        right.values[3][0], right.values[3][1], right.values[3][2],
        right.values[3][0] - rightTarget[0],
        right.values[3][1] - rightTarget[1],
        right.values[3][2] - rightTarget[2],
        leftValid ? 1 : 0, leftTarget[0], leftTarget[1], leftTarget[2],
        left.values[3][0], left.values[3][1], left.values[3][2]);
    appendLog_(message.data());
    lastReportAt_ = now;
    ++reports_;
}
} // namespace bfvr
