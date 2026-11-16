#include "DayNightCycle.h"
#include <algorithm>
#include <cmath>
using namespace DirectX;
namespace
{
float Smooth(float a, float b, float value)
{
    float t = std::clamp((value - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
XMFLOAT4 Mix(XMFLOAT4 a, XMFLOAT4 b, float t)
{
    return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t), 1};
}
} // namespace
void DayNightCycle::SetHour(float hour)
{
    if (!std::isfinite(hour))
        return;
    m_hour = std::fmod(hour, 24.f);
    if (m_hour < 0)
        m_hour += 24.f;
}
void DayNightCycle::AdvanceHours(float hours)
{
    if (!std::isfinite(hours))
        return;
    m_days += int(std::floor((m_hour + hours) / 24.f));
    SetHour(m_hour + hours);
}
void DayNightCycle::Update(float dt)
{
    if (std::isfinite(dt) && dt > 0)
        AdvanceHours(dt * 24.f / DayLengthSeconds);
}
Daylight DayNightCycle::Evaluate(FXMVECTOR position) const
{
    float angle = (m_hour - 6.f) * XM_2PI / 24.f;
    auto sun = XMVectorSet(std::cos(angle) * .6f, std::sin(angle), std::cos(angle) * .8f, 0);
    auto up = XMVector3Normalize(position);
    float elevation = XMVectorGetX(XMVector3Dot(up, sun));
    float day = Smooth(-.12f, .18f, elevation);
    float strength = Smooth(-.06f, .2f, elevation);
    float twilight = std::exp(-std::pow(elevation / .19f, 2.f));
    auto color = Mix({1.f, .29f, .055f, 1}, {1.f, .96f, .86f, 1}, Smooth(.03f, .4f, elevation));
    Daylight result;
    XMStoreFloat4(&result.sunDirection, sun);
    result.sunDirection.w = day;
    result.sunColor = {color.x * strength, color.y * strength, color.z * strength, .18f + .065f * day};
    auto horizon = Mix({.035f, .055f, .09f, 1}, {.43f, .68f, .86f, 1}, day);
    // Güneş ufka yaklaşınca hem gökyüzü hem sis sıcak renge geçer.
    // Warm the sky and fog together as the sun approaches the horizon.
    result.horizonColor = Mix(horizon, {.62f, .24f, .09f, 1}, twilight * .78f);
    return result;
}
