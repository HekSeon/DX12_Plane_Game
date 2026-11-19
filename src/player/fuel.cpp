#include "fuel.h"
#include <algorithm>
void FuelTank::Burn(float seconds, float throttle)
{
    m_level = std::max(0.f, m_level - std::max(0.f, seconds) * std::clamp(throttle, 0.f, 1.f) * FullThrottleBurn);
}
bool FuelTank::Fill(float seconds)
{
    float previous = m_level;
    m_level = std::min(Capacity, m_level + std::max(0.f, seconds) * RefuelRate);
    return m_level > previous;
}
