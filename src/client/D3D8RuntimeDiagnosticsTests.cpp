#include "client/D3D8RuntimeDiagnostics.h"
#include "client/BFSoldierArmBasisDiagnostics.h"

#include <cstdio>
#include <string>

namespace
{
unsigned reports = 0;
std::wstring lastReport;
void Capture(const wchar_t* message)
{
    ++reports;
    lastReport = message;
}

bool TestArmBasisDiagnostics() noexcept
{
    constexpr wchar_t key[] = L"BFVR_DIAGNOSTICS";
    const DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
    std::wstring saved(size, L'\0');
    if (size != 0) GetEnvironmentVariableW(key, saved.data(), size);
    auto identity = bfvr::native_arm_math::IdentityMatrix();
    auto solved = identity;
    solved.values[3][0] = 0.25F;
    const std::array<float, 3> target = {0.20F, 0.0F, 0.0F};
    bfvr::BFSoldierArmBasisDiagnostics diagnostics;
    reports = 0;
    bool passed = true;
    for (const wchar_t* level : {L"off", L"normal"})
    {
        if (!SetEnvironmentVariableW(key, level)) passed = false;
        diagnostics.Configure(Capture);
        diagnostics.Observe(&identity, identity, target, &solved,
            {}, nullptr, 0.0F, false, 0.0F, false, 7, nullptr);
        passed = passed && reports == 0;
    }
    if (!SetEnvironmentVariableW(key, L"deep")) passed = false;
    diagnostics.Configure(Capture);
    diagnostics.Observe(&identity, identity, target, &solved,
        {}, nullptr, 0.0F, false, 0.0F, false, 7, nullptr);
    passed = passed && reports == 1 &&
        lastReport.find(L"ARM_BASIS generation=7") != std::wstring::npos &&
        lastReport.find(L"rightError=(0.0500,0.0000,0.0000)") != std::wstring::npos &&
        lastReport.find(L"observedBodyYawValid=0") != std::wstring::npos &&
        lastReport.find(L"leftValid=0") != std::wstring::npos;
    diagnostics.Configure(Capture);
    diagnostics.Observe(nullptr, identity, target, &solved,
        {}, nullptr, 0.0F, false, 0.0F, false, 8, nullptr);
    passed = passed && reports == 1;
    diagnostics.Configure(nullptr);
    diagnostics.Observe(&identity, identity, target, &solved,
        {}, nullptr, 0.0F, false, 0.0F, false, 9, nullptr);
    passed = passed && reports == 1;
    if (!SetEnvironmentVariableW(key, size == 0 ? nullptr : saved.c_str())) passed = false;
    return passed;
}
} // namespace

int wmain()
{
    using bfvr::D3D8RuntimeDiagnosticLevel;
    using bfvr::IsOffHandCalibrationAuditMessage;
    using bfvr::ParseD3D8RuntimeDiagnosticLevel;

    const bool passed =
        ParseD3D8RuntimeDiagnosticLevel(L"off") ==
            D3D8RuntimeDiagnosticLevel::Off &&
        ParseD3D8RuntimeDiagnosticLevel(L"OFF") ==
            D3D8RuntimeDiagnosticLevel::Off &&
        ParseD3D8RuntimeDiagnosticLevel(L"0") ==
            D3D8RuntimeDiagnosticLevel::Off &&
        ParseD3D8RuntimeDiagnosticLevel(L"") ==
            D3D8RuntimeDiagnosticLevel::Normal &&
        ParseD3D8RuntimeDiagnosticLevel(L"normal") ==
            D3D8RuntimeDiagnosticLevel::Normal &&
        ParseD3D8RuntimeDiagnosticLevel(L"unknown") ==
            D3D8RuntimeDiagnosticLevel::Normal &&
        ParseD3D8RuntimeDiagnosticLevel(L"deep") ==
            D3D8RuntimeDiagnosticLevel::Deep &&
        ParseD3D8RuntimeDiagnosticLevel(L"DEEP") ==
            D3D8RuntimeDiagnosticLevel::Deep &&
        ParseD3D8RuntimeDiagnosticLevel(L"1") ==
            D3D8RuntimeDiagnosticLevel::Deep &&
        IsOffHandCalibrationAuditMessage(
            L"OFFHAND_CALIBRATION_CAPTURE sequence=1") &&
        IsOffHandCalibrationAuditMessage(
            L"OFFHAND_CALIBRATION_REJECT reason=test") &&
        !IsOffHandCalibrationAuditMessage(L"OFFHAND_CALIBRATION") &&
        !IsOffHandCalibrationAuditMessage(
            L"Native 1P off-hand support acquired");
    if (!passed || !TestArmBasisDiagnostics())
    {
        std::fwprintf(stderr, L"BFVR diagnostics-level parsing failed.\n");
        return 1;
    }
    std::wprintf(L"BFVR diagnostics-level tests passed.\n");
    return 0;
}
