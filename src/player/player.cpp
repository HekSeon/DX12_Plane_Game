#include "player.h"
#include "world/WorldGenerator.h"
#include "world/Airport.h"
#include <algorithm>
#include <cmath>
using namespace DirectX;
Player::Player()
{
    Reset();
}
void Player::Reset(bool onRunway, const WorldGenerator *world)
{
    auto airportFrame = Airport::Frame(0, world ? world->Seed() : 0);
    m_position = XMVector3TransformNormal(
        WorldGenerator::AirportPoint(0, onRunway ? -500.f : -1100.f, onRunway ? 30.f : 180.f), airportFrame);
    if (world && !onRunway)
    {
        auto direction = XMVector3Normalize(m_position);
        m_position = direction * (WorldGenerator::Radius + std::max(180.f, world->GroundHeight(direction) + 160.f));
    }
    auto up = XMVector3Normalize(m_position);
    auto right = XMVector3Normalize(XMVector3Cross(up, airportFrame.r[2]));
    m_forward = XMVector3Normalize(XMVector3Cross(right, up));
    m_speed = onRunway ? 0.f : 100.f;
    m_throttle = onRunway ? 0.f : .65f;
    m_bank = 0;
    m_fuel.Reset();
    m_refueling = false;
    m_braking = false;
    m_serviceAirport = -1;
    m_onGround = onRunway;
    m_camera = m_position - m_forward * 65 + up * 22;
}
float Player::Altitude() const
{
    return XMVectorGetX(XMVector3Length(m_position)) - WorldGenerator::Radius;
}
// Küresel dünyada uçağın yukarı yönü bulunduğu konuma göre değişir.
// On a spherical world, the aircraft's up axis depends on its position.
XMMATRIX Player::WorldMatrix() const
{
    auto up = XMVector3Normalize(m_position);
    auto right = XMVector3Normalize(XMVector3Cross(up, m_forward));
    auto localUp = XMVector3Normalize(XMVector3Cross(m_forward, right));
    XMMATRIX orientation(right, localUp, m_forward, XMVectorSetW(m_position, 1));
    return XMMatrixRotationZ(m_bank) * orientation;
}
void Player::Update(float delta, HWND window, const WorldGenerator &world)
{
    auto key = [&](int code) { return GetForegroundWindow() == window && (GetAsyncKeyState(code) & 0x8000); };
    PlayerInput input;
    input.pitch = float(key('S') || key(VK_DOWN)) - float(key('W') || key(VK_UP));
    input.roll = float(key('A') || key(VK_LEFT)) - float(key('D') || key(VK_RIGHT));
    input.throttle = float(key(VK_SHIFT)) - float(key(VK_CONTROL));
    input.brake = key(VK_SPACE);
    input.refuel = key('E');
    Update(delta, input, world);
}

// Klavye okumasını fizik adımından ayır; simülasyon giriş kaynağına bağlı kalmasın.
// Separate keyboard polling from physics so simulation does not depend on the input source.
void Player::Update(float delta, const PlayerInput &input, const WorldGenerator &world)
{
    float dt = std::clamp(delta, 0.f, .05f);
    if (input.reset && !m_resetDown)
        Reset(false, &world);
    if (input.runway && !m_homeDown)
        Reset(true, &world);
    m_resetDown = input.reset;
    m_homeDown = input.runway;
    float pitch = std::clamp(input.pitch, -1.f, 1.f), roll = std::clamp(input.roll, -1.f, 1.f);
    m_throttle = std::clamp(m_throttle + std::clamp(input.throttle, -1.f, 1.f) * dt * .3f, 0.f, 1.f);
    // Ctrl yerde gazı kesip fren uygular; havada yalnızca gazı azaltır.
    // Ctrl cuts power and applies brakes on the ground; in flight it only reduces throttle.
    m_braking = m_onGround && (input.brake || input.throttle < 0);
    if (m_onGround && input.throttle < 0)
        m_throttle = 0;
    m_fuel.Burn(dt, m_throttle);
    if (m_fuel.Percent() <= 0)
        m_throttle = 0;
    auto up = XMVector3Normalize(m_position);
    auto right = XMVector3Normalize(XMVector3Cross(up, m_forward));
    // Kalkış hızında burun çekilince zemin kilidini bırak; pitch birikebilsin.
    // Release ground lock when rotating at takeoff speed so pitch can accumulate.
    if (m_onGround && m_speed > 55.f && pitch > 0.f)
        m_onGround = false;
    float bankTarget = m_onGround ? 0.f : roll * .85f;
    m_bank += (bankTarget - m_bank) * (1 - std::exp(-dt * 3));
    if (!m_onGround || m_speed > 55.f)
        m_forward =
            XMVector3Normalize(XMVector3TransformNormal(m_forward, XMMatrixRotationAxis(right, -pitch * dt * .55f)));
    m_forward = XMVector3Normalize(XMVector3TransformNormal(
        m_forward, XMMatrixRotationAxis(up, (m_onGround ? -roll * .4f : -m_bank * .45f) * dt)));
    float climb = XMVectorGetX(XMVector3Dot(m_forward, up));
    float drag = m_onGround ? 1.2f : 4.f;
    m_speed = std::clamp(m_speed + (m_throttle * 22 - drag - m_speed * .065f - climb * 9.81f) * dt, 0.f, 230.f);
    if (m_braking)
        m_speed = std::max(0.f, m_speed - 35.f * dt);
    // Düşük hızda kaldırma azalır; burun yukarıda olsa da uçak irtifa kaybeder.
    // Low airspeed reduces lift, so the aircraft sinks even with the nose raised.
    float sink = m_onGround ? 0.f : std::max(0.f, 65.f - m_speed) * .9f;
    m_position += m_forward * (m_speed * dt) - up * (sink * dt);
    auto newUp = XMVector3Normalize(m_position);
    auto axis = XMVector3Cross(up, newUp);
    float sine = XMVectorGetX(XMVector3Length(axis));
    if (sine > 1e-7f)
        m_forward = XMVector3Normalize(XMVector3Rotate(
            m_forward, XMQuaternionRotationAxis(axis, std::atan2(sine, XMVectorGetX(XMVector3Dot(up, newUp))))));
    float ground = world.GroundHeight(newUp);
    float floor = WorldGenerator::Radius + ground + 4.f;
    float radius = XMVectorGetX(XMVector3Length(m_position));
    if (radius < floor || (m_onGround && climb < .03f))
    {
        m_position = newUp * floor;
        m_forward = XMVector3Normalize(m_forward - newUp * XMVectorGetX(XMVector3Dot(m_forward, newUp)));
        m_onGround = true;
    }
    else if (climb > .03f && m_speed > 55.f)
        m_onGround = false;
    m_serviceAirport = Airport::RefuelZone(m_position, m_onGround, m_speed, m_throttle, world.Seed());
    m_refueling = input.refuel && m_serviceAirport >= 0 && m_fuel.Fill(dt);
    m_camera = XMVectorLerp(m_camera, m_position - m_forward * 65 + newUp * 22, 1 - std::exp(-dt * 5));
}
