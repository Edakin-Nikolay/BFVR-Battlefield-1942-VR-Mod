#include "stereo/CloseRangeAimMath.h"

#include <algorithm>
#include <cmath>

namespace bfvr::stereo
{
namespace
{
constexpr float kPi = 3.14159265358979323846F;
constexpr float kEpsilon = 0.000001F;

bool Finite(float v) noexcept
{
    return std::isfinite(v);
}

bool FiniteVec(const Vec3& v) noexcept
{
    return Finite(v.x) && Finite(v.y) && Finite(v.z);
}

float Dot(const Vec3& a, const Vec3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Add(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(const Vec3& a, float s) noexcept
{
    return {a.x * s, a.y * s, a.z * s};
}

std::optional<Vec3> Normalize(const Vec3& v) noexcept
{
    const float len = std::sqrt(Dot(v, v));
    if (!Finite(len) || len < kEpsilon)
    {
        return std::nullopt;
    }
    return Scale(v, 1.0F / len);
}

Vec3 Row(const Matrix4& m, int i) noexcept
{
    return {m.values[i][0], m.values[i][1], m.values[i][2]};
}

Vec3 Rotate(const Matrix4& m, const Vec3& v) noexcept
{
    return Add(Add(Scale(Row(m, 0), v.x), Scale(Row(m, 1), v.y)),
        Scale(Row(m, 2), v.z));
}
} // namespace

std::optional<CloseRangeAimParallax> MeasureCloseRangeAimParallax(
    const Matrix4& gunWorld,
    const Vec3& barrelOffsetLocal,
    const Vec3& barrelDirectionLocal,
    const float planeDistanceMeters) noexcept
{
    if (!Finite(planeDistanceMeters) || planeDistanceMeters <= 0.0F ||
        !FiniteVec(barrelOffsetLocal) || !FiniteVec(barrelDirectionLocal))
    {
        return std::nullopt;
    }
    for (const auto& row : gunWorld.values)
    {
        for (const float v : row)
        {
            if (!Finite(v))
            {
                return std::nullopt;
            }
        }
    }
    const auto forward = Normalize(Row(gunWorld, 2));
    const auto barrelDir = Normalize(Rotate(gunWorld, barrelDirectionLocal));
    if (!forward.has_value() || !barrelDir.has_value())
    {
        return std::nullopt;
    }
    const Vec3 right = Row(gunWorld, 0);
    const Vec3 up = Row(gunWorld, 1);
    const Vec3 origin = Row(gunWorld, 3);
    const Vec3 barrelOffsetWorld = Rotate(gunWorld, barrelOffsetLocal);

    const float denominator = Dot(*barrelDir, *forward);
    if (denominator < kEpsilon)
    {
        return std::nullopt;
    }
    const float s =
        (planeDistanceMeters - Dot(barrelOffsetWorld, *forward)) / denominator;
    const Vec3 reticlePoint = Add(origin, Scale(*forward, planeDistanceMeters));
    const Vec3 bulletPoint =
        Add(Add(origin, barrelOffsetWorld), Scale(*barrelDir, s));
    const Vec3 offset = Sub(bulletPoint, reticlePoint);

    const float cosine = std::clamp(Dot(*forward, *barrelDir), -1.0F, 1.0F);
    CloseRangeAimParallax result = {};
    result.lateralMeters = Dot(offset, right);
    result.verticalMeters = Dot(offset, up);
    result.basisAngularDegrees = std::acos(cosine) * 180.0F / kPi;
    if (!Finite(result.lateralMeters) || !Finite(result.verticalMeters) ||
        !Finite(result.basisAngularDegrees))
    {
        return std::nullopt;
    }
    return result;
}

} // namespace bfvr::stereo
