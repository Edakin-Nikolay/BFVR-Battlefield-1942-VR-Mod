#pragma once

#include "client/BFSoldierNativeArmMath.h"

#include <windows.h>

#include <array>

namespace bfvr
{

// Read-only, bounded comparison of requested IK targets and the actual native
// solve. Enabled only by BFVR_DIAGNOSTICS=deep; never changes a transform.
class BFSoldierArmBasisDiagnostics final
{
public:
    void Configure(void (*appendLog)(const wchar_t*)) noexcept;
    void Observe(
        const stereo::Matrix4* skeletonRoot,
        const stereo::Matrix4& inverseSoldierWorld,
        const std::array<float, 3>& rightTarget,
        const stereo::Matrix4* rightSolved,
        const std::array<float, 3>& leftTarget,
        const stereo::Matrix4* leftSolved,
        LONG controllerGeneration,
        const void* soldier) noexcept;

private:
    void (*appendLog_)(const wchar_t*) = nullptr;
    DWORD lastReportAt_ = 0;
    unsigned reports_ = 0;
    bool enabled_ = false;
};

} // namespace bfvr
