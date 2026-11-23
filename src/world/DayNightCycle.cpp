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
    // HUD saati yerel ufku belirler; gezegen üzerindeki konum gün doğumunu kaydırmaz.
    // The HUD clock defines the local horizon; planet position cannot shift sunrise.
    bool daytime = m_hour >= SunriseHour && m_hour < SunsetHour;
    float nightHours = 24.f - (SunsetHour - SunriseHour);
    float elapsedNight = m_hour >= SunsetHour ? m_hour - SunsetHour : m_hour + 24.f - SunsetHour;
    float angle = daytime ? XM_PI * (m_hour - SunriseHour) / (SunsetHour - SunriseHour)
                          : XM_PI + XM_PI * elapsedNight / nightHours;
    auto up = XMVector3Normalize(position);
    auto reference = std::abs(XMVectorGetZ(up)) > .98f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 0, 1, 0);
    auto east = XMVector3Normalize(XMVector3Cross(up, reference));
    float elevation = std::sin(angle);
    auto sun = XMVector3Normalize(east * std::cos(angle) + up * elevation);
    float day = daytime ? Smooth(0.f, .15f, elevation) : 0.f;
    float strength = daytime ? Smooth(0.f, .2f, elevation) : 0.f;
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
