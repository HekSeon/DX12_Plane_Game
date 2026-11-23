#pragma once
#include <DirectXMath.h>

struct Daylight
{
    DirectX::XMFLOAT4 sunDirection;
    DirectX::XMFLOAT4 sunColor;
    DirectX::XMFLOAT4 horizonColor;
};

// Gökyüzü ve sahne ışığı aynı güneş konumundan hesaplanır.
// Compute the sky and scene lighting from the same sun position.
class DayNightCycle
{
  public:
    static constexpr float DayLengthSeconds = 600.f;
    static constexpr float SunriseHour = 6.f;
    static constexpr float SunsetHour = 18.f;
    void Update(float dt);
    void AdvanceHours(float hours);
    void SetHour(float hour);
    float Hour() const { return m_hour; }
    int DaysElapsed() const { return m_days; }
    Daylight Evaluate(DirectX::FXMVECTOR position) const;

  private:
    float m_hour = 6.5f;
    int m_days = 0;
};
