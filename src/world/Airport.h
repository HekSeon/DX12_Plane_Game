#pragma once
#include "model/mesh.h"
#include <vector>
struct AirportPart
{
    MeshData mesh;
    DirectX::XMFLOAT4 color;
};
// Pist, taksi yolu, hangarlar ve kule için sabit sahne geometrisi üretir.
// Build static scene geometry for the runway, taxiway, hangars and tower.
class Airport
{
  public:
    static constexpr int Count = 2;
    static const char *Name(int index);
    static DirectX::XMMATRIX Frame(int index, uint32_t seed = 0);
    static DirectX::XMVECTOR Center(int index, uint32_t seed = 0);
    static DirectX::XMVECTOR LocalPosition(DirectX::FXMVECTOR position, int index, uint32_t seed = 0);
    static float Distance(DirectX::FXMVECTOR position, int index, uint32_t seed = 0);
    static bool OnPavement(DirectX::FXMVECTOR position, uint32_t seed = 0);
    static int RefuelZone(DirectX::FXMVECTOR position, bool grounded, float speed, float throttle, uint32_t seed = 0);
    static std::vector<AirportPart> Generate(uint32_t seed = 0);
};
