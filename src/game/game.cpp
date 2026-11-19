#include "game.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <set>
using namespace DirectX;
Game::Game() : m_world(std::random_device{}()) {}
void Game::Initialize()
{
    m_player.Reset(true, &m_world);
    m_route.Reset();
    m_airport = Airport::Generate(m_world.Seed());
    RefreshChunks(true);
    m_fog.color = Lighting().horizonColor;
}
void Game::RefreshChunks(bool initial)
{
    m_requested = m_world.VisibleChunks(m_player.Position(), m_player.Forward());
    std::set<ChunkId> required(m_requested.begin(), m_requested.end());
    for (auto it = m_chunks.begin(); it != m_chunks.end();)
    {
        if (!required.contains(it->first))
            it = m_chunks.erase(it);
        else
            ++it;
    }
    if (initial)
    {
        auto surface = XMVector3Normalize(m_player.Position()) * WorldGenerator::Radius;
        for (auto id : m_requested)
            if (XMVectorGetX(XMVector3Length(m_world.ChunkCenter(id) - surface)) < 2700.f)
                m_chunks.emplace(id, m_world.GenerateChunk(id));
    }
}
float Game::TargetBearing() const
{
    auto up = XMVector3Normalize(m_player.Position());
    auto heading = XMVector3Normalize(m_player.Forward() - up * XMVectorGetX(XMVector3Dot(m_player.Forward(), up)));
    auto right = XMVector3Cross(up, heading);
    auto target = Airport::Center(TargetAirport(), m_world.Seed());
    auto tangent = target - up * XMVectorGetX(XMVector3Dot(target, up));
    if (XMVectorGetX(XMVector3LengthSq(tangent)) < 1e-8f)
        return 0;
    return std::atan2(XMVectorGetX(XMVector3Dot(tangent, right)), XMVectorGetX(XMVector3Dot(tangent, heading)));
}
void Game::Update(float dt, HWND window)
{
    bool restart = GetForegroundWindow() == window && (GetAsyncKeyState('R') & 0x8000);
    if (restart && !m_restartDown)
    {
        // Yeniden başlatma yakıtla birlikte görev ilerlemesini de sıfırlar.
        // Restart resets route progress together with the fuel tank.
        m_route.Reset();
        m_player.Reset(true, &m_world);
        m_arrivalTimer = 0;
        RefreshChunks(true);
    }
    m_restartDown = restart;
    m_player.Update(dt, window, m_world);
    m_arrivalTimer = std::max(0.f, m_arrivalTimer - std::clamp(dt, 0.f, .05f));
    if (m_route.Arrive(m_player.FuelServiceAirport()))
        m_arrivalTimer = 6.f;
    m_seaTime += std::clamp(dt, 0.f, .05f);
    // Uzun bir pencere duraklaması günün saatini bir anda değiştirmesin.
    // Avoid a large time jump after the window has been paused.
    m_dayNight.Update(std::clamp(dt, 0.f, .25f));
    bool advance = GetForegroundWindow() == window && (GetAsyncKeyState('T') & 0x8000);
    if (advance && !m_timeDown)
        m_dayNight.AdvanceHours(3.f);
    m_timeDown = advance;
    m_fog.color = Lighting().horizonColor;
    bool fog = GetForegroundWindow() == window && (GetAsyncKeyState('F') & 0x8000);
    if (fog && !m_fogDown)
        m_fog.enabled = !m_fog.enabled;
    m_fogDown = fog;
    bool mute = GetForegroundWindow() == window && (GetAsyncKeyState('M') & 0x8000);
    if (mute && !m_muteDown)
        m_soundEnabled = !m_soundEnabled;
    m_muteDown = mute;
    m_streamTimer += dt;
    if (m_streamTimer > .25f)
    {
        m_streamTimer = 0;
        RefreshChunks(false);
    }
    // Üretimi karelere yayarak yeni bölgelerde oluşan takılmayı azalt.
    // Spread generation across frames to reduce stalls in new areas.
    int budget = 4;
    for (auto id : m_requested)
        if (!m_chunks.contains(id))
        {
            m_chunks.emplace(id, m_world.GenerateChunk(id));
            if (--budget == 0)
                break;
        }
    m_titleTimer += dt;
    if (m_titleTimer > .2f)
    {
        m_titleTimer = 0;
        wchar_t title[512];
        float distance = Airport::Distance(m_player.Position(), TargetAirport(), m_world.Seed());
        swprintf_s(title,
                   L"DX12 Plane | %.0f km/h | Alt %.0f m | Throttle %.0f%% | Airport %.1f km | Chunks %zu | Seed %u | "
                   L"%s | %02d:%02d | T +3h  F fog  Ctrl brake  R restart route",
                   m_player.Speed() * 3.6f, m_player.Altitude(), m_player.Throttle() * 100, distance / 1000,
                   m_chunks.size(), m_world.Seed(),
                   m_player.Braking()    ? L"BRAKE"
                   : m_player.OnGround() ? L"GROUND"
                   : m_player.Stalled()  ? L"STALL"
                                         : L"FLIGHT",
                   int(m_dayNight.Hour()), int(m_dayNight.Hour() * 60) % 60);
        SetWindowTextW(window, title);
    }
}
