#pragma once
#include "world/Airport.h"

// Hedef yalnızca atanmış meydanın park alanına ulaşınca değişir.
// Change the destination only after reaching the assigned airport's parking bay.
class FlightRoute
{
  public:
    void Reset()
    {
        m_target = 1;
        m_completed = 0;
    }
    int Target() const { return m_target; }
    unsigned Completed() const { return m_completed; }
    bool Arrive(int serviceAirport)
    {
        if (serviceAirport != m_target)
            return false;
        ++m_completed;
        m_target = (m_target + 1) % Airport::Count;
        return true;
    }

  private:
    int m_target = 1;
    unsigned m_completed = 0;
};
