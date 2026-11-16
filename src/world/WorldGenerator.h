#pragma once
#include "model/mesh.h"
#include <cstdint>
#include <vector>

struct ChunkId
{
    int face, x, y;
    auto operator<=>(const ChunkId &) const = default;
};

// Aynı seed ve yön her zaman aynı yüksekliği verir; geri dönüşte arazi değişmez.
// The same seed and direction always produce the same height on return visits.
class WorldGenerator
{
  public:
    static constexpr float Radius = 5000.f;
    static constexpr int TilesPerFace = 32;
    static constexpr int TileSegments = 16;
    explicit WorldGenerator(uint32_t seed);
    uint32_t Seed() const { return m_seed; }
    float Height(DirectX::FXMVECTOR direction) const;
    float GroundHeight(DirectX::FXMVECTOR direction) const;
    MeshData GenerateChunk(ChunkId id) const;
    std::vector<ChunkId> VisibleChunks(DirectX::FXMVECTOR position, DirectX::FXMVECTOR forward) const;
    DirectX::XMVECTOR ChunkCenter(ChunkId id) const;
    static DirectX::XMVECTOR CubeDirection(int face, float u, float v);
    static DirectX::XMVECTOR AirportPoint(float x, float z, float height);

  private:
    float Noise(float x, float y, float z) const;
    uint32_t m_seed;
};
