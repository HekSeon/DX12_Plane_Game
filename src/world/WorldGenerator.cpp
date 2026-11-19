#include "WorldGenerator.h"
#include "Airport.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace DirectX;
namespace
{
float Smooth(float v)
{
    v = std::clamp(v, 0.f, 1.f);
    return v * v * (3 - 2 * v);
}
uint32_t Hash(uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352d;
    v ^= v >> 15;
    v *= 0x846ca68b;
    return v ^ (v >> 16);
}
} // namespace
WorldGenerator::WorldGenerator(uint32_t seed) : m_seed(seed) {}

float WorldGenerator::Noise(float x, float y, float z) const
{
    int ix = int(std::floor(x)), iy = int(std::floor(y)), iz = int(std::floor(z));
    float tx = Smooth(x - ix), ty = Smooth(y - iy), tz = Smooth(z - iz);
    auto sample = [&](int a, int b, int c) {
        uint32_t hash = Hash(uint32_t(a) * 73856093u ^ uint32_t(b) * 19349663u ^ uint32_t(c) * 83492791u ^ m_seed);
        return float(hash & 0xffffff) / float(0xffffff) * 2 - 1;
    };
    auto layer = [&](int c) {
        return std::lerp(std::lerp(sample(ix, iy, c), sample(ix + 1, iy, c), tx),
                         std::lerp(sample(ix, iy + 1, c), sample(ix + 1, iy + 1, c), tx), ty);
    };
    return std::lerp(layer(iz), layer(iz + 1), tz);
}

float WorldGenerator::Height(FXMVECTOR direction) const
{
    XMFLOAT3 n;
    XMStoreFloat3(&n, XMVector3Normalize(direction));
    float continent = Noise(n.x * 4, n.y * 4, n.z * 4);
    float hills = Noise(n.x * 16, n.y * 16, n.z * 16) * .65f + Noise(n.x * 32, n.y * 32, n.z * 32) * .25f;
    float mountains = std::pow(std::max(0.f, 1 - std::abs(Noise(n.x * 10, n.y * 10, n.z * 10))), 3.f);
    float height = continent * 320.f - 35.f + std::max(0.f, continent + .1f) * (180.f * mountains + 95.f * hills);
    // Havalimanının çevresini düzleştir; kenarları araziye yumuşak bağla.
    // Flatten the airfield and blend its edges into the terrain.
    for (int airport = 0; airport < Airport::Count; ++airport)
    {
        auto local = Airport::LocalPosition(direction * Radius, airport, m_seed);
        float x = XMVectorGetX(local), z = XMVectorGetZ(local);
        float edge = std::max((std::abs(x) - 200.f) / 160.f, (std::abs(z) - 760.f) / 220.f);
        height = std::lerp(24.f, height, Smooth(edge));
    }
    return height;
}
float WorldGenerator::GroundHeight(FXMVECTOR direction) const
{
    if (Airport::OnPavement(direction, m_seed))
        return 26.f;
    return std::max(0.f, Height(direction));
}

XMVECTOR WorldGenerator::CubeDirection(int face, float u, float v)
{
    XMVECTOR p;
    switch (face)
    {
    case 0:
        p = XMVectorSet(1, v, -u, 0);
        break;
    case 1:
        p = XMVectorSet(-1, v, u, 0);
        break;
    case 2:
        p = XMVectorSet(u, 1, -v, 0);
        break;
    case 3:
        p = XMVectorSet(u, -1, v, 0);
        break;
    case 4:
        p = XMVectorSet(u, v, 1, 0);
        break;
    case 5:
        p = XMVectorSet(-u, v, -1, 0);
        break;
    default:
        throw std::runtime_error("Invalid cube face");
    }
    return XMVector3Normalize(p);
}
XMVECTOR WorldGenerator::ChunkCenter(ChunkId id) const
{
    return CubeDirection(id.face, -1.f + 2.f * (id.x + .5f) / TilesPerFace, -1.f + 2.f * (id.y + .5f) / TilesPerFace) *
           Radius;
}
XMVECTOR WorldGenerator::AirportPoint(float x, float z, float height)
{
    return XMVector3Normalize(XMVectorSet(x, Radius, z, 0)) * (Radius + height);
}
MeshData WorldGenerator::GenerateChunk(ChunkId id) const
{
    if (id.x < 0 || id.x >= TilesPerFace || id.y < 0 || id.y >= TilesPerFace)
        throw std::runtime_error("Invalid terrain tile");
    MeshData mesh;
    constexpr int S = TileSegments;
    mesh.vertices.reserve((S + 1) * (S + 1));
    mesh.indices.reserve(S * S * 6);
    for (int y = 0; y <= S; ++y)
        for (int x = 0; x <= S; ++x)
        {
            // Ortak kenarlar aynı tam sayı koordinatlarından örneklenir.
            // Shared edges are sampled from the same integer coordinates.
            float u = -1.f + 2.f * float(id.x * S + x) / (TilesPerFace * S);
            float v = -1.f + 2.f * float(id.y * S + y) / (TilesPerFace * S);
            auto n = CubeDirection(id.face, u, v);
            auto tangent =
                XMVector3Cross(std::abs(XMVectorGetY(n)) > .95f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0), n);
            tangent = XMVector3Normalize(tangent);
            auto bitangent = XMVector3Cross(n, tangent);
            auto point = [&](XMVECTOR d) {
                d = XMVector3Normalize(d);
                return d * (Radius + Height(d));
            };
            auto a = point(n + tangent * .0004f) - point(n - tangent * .0004f);
            auto b = point(n + bitangent * .0004f) - point(n - bitangent * .0004f);
            auto normal = XMVector3Normalize(XMVector3Cross(a, b));
            Vertex vertex{};
            XMStoreFloat3(&vertex.position, point(n));
            XMStoreFloat3(&vertex.normal, normal);
            // Arazi dokusu shader içinde dünya koordinatlarıyla kaplanır.
            // The shader maps terrain textures in world space.
            vertex.uv = {u, v};
            mesh.vertices.push_back(vertex);
        }
    for (uint32_t y = 0; y < S; ++y)
        for (uint32_t x = 0; x < S; ++x)
        {
            uint32_t a = y * (S + 1) + x, b = a + S + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return mesh;
}
std::vector<ChunkId> WorldGenerator::VisibleChunks(FXMVECTOR position, FXMVECTOR forward) const
{
    auto surface = XMVector3Normalize(position) * Radius;
    auto up = XMVector3Normalize(position);
    auto heading = forward - up * XMVectorGetX(XMVector3Dot(forward, up));
    if (XMVectorGetX(XMVector3LengthSq(heading)) < 1e-6f)
        heading = XMVectorSet(0, 0, 1, 0);
    heading = XMVector3Normalize(heading);
    struct Candidate
    {
        ChunkId id;
        float priority;
    };
    std::vector<Candidate> candidates;
    for (int face = 0; face < 6; ++face)
        for (int y = 0; y < TilesPerFace; ++y)
            for (int x = 0; x < TilesPerFace; ++x)
            {
                ChunkId id{face, x, y};
                auto delta = ChunkCenter(id) - surface;
                float distance = XMVectorGetX(XMVector3Length(delta));
                float ahead = distance > 1.f ? XMVectorGetX(XMVector3Dot(delta / distance, heading)) : 0;
                // Yakın çevreyi koru, uçuş yönündeki parçaları daha erken hazırla.
                // Keep nearby tiles and prefetch farther ahead of the aircraft.
                if (distance < 2900.f || (distance < 4100.f && ahead > .2f))
                    candidates.push_back({id, distance * (1 - .35f * std::max(0.f, ahead))});
            }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.priority < b.priority; });
    std::vector<ChunkId> result;
    result.reserve(candidates.size());
    for (auto &candidate : candidates)
        result.push_back(candidate.id);
    return result;
}
