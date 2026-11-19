#pragma once
// Tam depo, ALPHA-BRAVO gidiş-dönüşü ve yaklaşma payını karşılar.
// A full tank covers the ALPHA-BRAVO round trip with approach reserves.
class FuelTank
{
  public:
    static constexpr float Capacity = 100.f;
    static constexpr float FullThrottleBurn = .45f;
    static constexpr float RefuelRate = 12.f;
    float Percent() const { return m_level; }
    void Reset() { m_level = Capacity; }
    void Burn(float seconds, float throttle);
    bool Fill(float seconds);

  private:
    float m_level = Capacity;
};
