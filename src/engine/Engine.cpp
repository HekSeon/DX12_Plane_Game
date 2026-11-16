#include "Engine.h"
#include <chrono>
#include <filesystem>

Engine::~Engine()
{
    m_sound.Shutdown();
    if (m_renderer)
        m_renderer->Shutdown();
}

bool Engine::Initialize()
{
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!std::filesystem::exists("data"))
        std::filesystem::current_path(std::filesystem::path(executable).parent_path());
    SetProcessDPIAware();
    if (!m_window.Create(L"DX12 Plane Game", 1280, 720))
        return false;
    m_game.Initialize();
    m_renderer = CreateRenderer();
    if (!m_renderer->Initialize(m_window.GetHandle(), m_window.Width(), m_window.Height(), m_game))
        return false;
    m_sound.Initialize();
    return true;
}

int Engine::Run()
{
    if (!Initialize())
        return 1;
    auto previous = std::chrono::steady_clock::now();
    while (m_window.PumpMessages())
    {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        if (IsIconic(m_window.GetHandle()))
        {
            m_sound.Update(0, 0, false);
            WaitMessage();
            continue;
        }
        Update(dt);
        Render();
    }
    return 0;
}

void Engine::Update(float dt)
{
    uint32_t width = 0, height = 0;
    if (m_window.ConsumeResize(width, height) && width && height)
        m_renderer->Resize(width, height);
    // Önce sahneyi güncelle; renderer ve ses aynı oyuncu durumunu kullansın.
    // Update the scene first so rendering and sound use the same player state.
    m_game.Update(dt, m_window.GetHandle());
    const auto &player = m_game.GetPlayer();
    m_sound.Update(player.Throttle(), player.Speed(),
                   m_game.SoundEnabled() && GetForegroundWindow() == m_window.GetHandle());
}

void Engine::Render()
{
    m_renderer->Render();
}
