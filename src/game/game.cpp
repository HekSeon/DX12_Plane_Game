#include "game.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <set>
using namespace DirectX;
Game::Game() : m_world(std::random_device{}()) {}
void Game::Initialize()
{
    m_player.Reset(false, &m_world);
    m_airport = Airport::Generate();
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
void Game::Update(float dt, HWND window)
{
    m_player.Update(dt, window, m_world);
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
        auto direction = XMVector3Normalize(m_player.Position());
        float distance = std::acos(std::clamp(XMVectorGetY(direction), -1.f, 1.f)) * WorldGenerator::Radius;
        swprintf_s(title,
                   L"DX12 Plane | %.0f km/h | Alt %.0f m | Throttle %.0f%% | Airport %.1f km | Chunks %zu | Seed %u | "
                   L"%s | %02d:%02d | T +3h  F fog  H runway  R reset",
                   m_player.Speed() * 3.6f, m_player.Altitude(), m_player.Throttle() * 100, distance / 1000,
                   m_chunks.size(), m_world.Seed(),
                   m_player.OnGround()  ? L"GROUND"
                   : m_player.Stalled() ? L"STALL"
                                        : L"FLIGHT",
                   int(m_dayNight.Hour()), int(m_dayNight.Hour() * 60) % 60);
        SetWindowTextW(window, title);
    }
}
