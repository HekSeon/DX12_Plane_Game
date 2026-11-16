#pragma once
#include "player/player.h"
#include "world/WorldGenerator.h"
#include "world/Airport.h"
#include "world/DayNightCycle.h"
#include <map>
#include <vector>

struct Fog
{
    float start = 650.f, end = 2400.f;
    DirectX::XMFLOAT4 color{.43f, .68f, .86f, 1};
    bool enabled = true;
};
// Dünya, oyuncu ve sahne ayarlarının sahibi. GPU kaynakları renderer içinde kalır.
// Own the world, player and scene settings; GPU resources stay in the renderer.
class Game
{
  public:
    Game();
    void Initialize();
    void Update(float dt, HWND window);
    const Player &GetPlayer() const { return m_player; }
    const WorldGenerator &World() const { return m_world; }
    const std::map<ChunkId, MeshData> &Chunks() const { return m_chunks; }
    const std::vector<AirportPart> &AirportParts() const { return m_airport; }
    const Fog &GetFog() const { return m_fog; }
    const DayNightCycle &TimeOfDay() const { return m_dayNight; }
    Daylight Lighting() const { return m_dayNight.Evaluate(m_player.Position()); }
    float SeaTime() const { return m_seaTime; }
    bool SoundEnabled() const { return m_soundEnabled; }

  private:
    void RefreshChunks(bool initial);
    WorldGenerator m_world;
    Player m_player;
    Fog m_fog;
    DayNightCycle m_dayNight;
    std::map<ChunkId, MeshData> m_chunks;
    std::vector<ChunkId> m_requested;
    std::vector<AirportPart> m_airport;
    float m_streamTimer = 0, m_titleTimer = 0, m_seaTime = 0;
    bool m_fogDown = false, m_muteDown = false, m_soundEnabled = true, m_timeDown = false;
};
