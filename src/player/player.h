#pragma once
#include <Windows.h>
#include <DirectXMath.h>
class WorldGenerator;

struct PlayerInput
{
    float pitch = 0, roll = 0, throttle = 0;
    bool brake = false, reset = false, runway = false;
};

// Uçak durumu, klavye kontrolleri ve takip kamerası burada güncellenir.
// Update aircraft state, keyboard controls and the chase camera here.
class Player
{
  public:
    Player();
    void Reset(bool onRunway = false, const WorldGenerator *world = nullptr);
    void Update(float dt, HWND window, const WorldGenerator &world);
    void Update(float dt, const PlayerInput &input, const WorldGenerator &world);
    DirectX::XMVECTOR Position() const { return m_position; }
    DirectX::XMVECTOR Forward() const { return m_forward; }
    DirectX::XMVECTOR Camera() const { return m_camera; }
    DirectX::XMMATRIX WorldMatrix() const;
    float Speed() const { return m_speed; }
    float Throttle() const { return m_throttle; }
    float Fuel() const { return m_fuel; }
    float Altitude() const;
    bool OnGround() const { return m_onGround; }
    bool Stalled() const { return !m_onGround && m_speed < 60.f; }

  private:
    DirectX::XMVECTOR m_position, m_forward, m_camera;
    float m_speed = 100, m_throttle = .65f, m_bank = 0, m_fuel = 100;
    bool m_onGround = false, m_resetDown = false, m_homeDown = false;
};
